#include "client.h"
#include <iostream>
#include <sstream>
#include <QDebug>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QDateTime>

void initLogging() {
    QFile logFile("client.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
            QFile logFile("client.log");
            if (logFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&logFile);
                out << QDateTime::currentDateTime().toString() << ": " << msg << "\n";
            }
        });
    }
}

Client* Client::p_instance = nullptr;
SingletonDestroyer Client::destroyer;

Client& Client::getInstance() {
    if (!p_instance) {
        p_instance = new Client();
        destroyer.initialize(p_instance);
    }
    return *p_instance;
}

SingletonDestroyer::~SingletonDestroyer() {
    delete p_instance;
}

void SingletonDestroyer::initialize(Client* p) {
    p_instance = p;
}

Client::Client() : is_connected_(false), socket_(INVALID_SOCKET), currentRoomId(-1), pendingRoomId(-1) {
    initLogging();
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        qDebug() << "WSAStartup failed";
        std::cerr << "WSAStartup failed.\n";
    }
}

Client::~Client() {
    disconnect();
    WSACleanup();
}

bool Client::connectToServer(const std::string& ip, int port) {
    qDebug() << "Attempting to connect to" << ip.c_str() << ":" << port;
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_connected_) {
        qDebug() << "Already connected";
        return true;
    }

    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        qDebug() << "Socket creation failed with error:" << WSAGetLastError();
        std::cerr << "Socket creation failed.\n";
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (::connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        qDebug() << "Connection failed with error:" << WSAGetLastError();
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        std::cerr << "Connection failed.\n";
        return false;
    }

    qDebug() << "Connected successfully";
    is_connected_ = true;
    receive_thread_ = std::thread(&Client::receiveThread, this);
    return true;
}

void Client::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_connected_) return;

    qDebug() << "Disconnecting";
    is_connected_ = false;
    if (socket_ != INVALID_SOCKET) {
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

void Client::sendRequest(const std::string& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_connected_) {
        qDebug() << "Cannot send request: not connected";
        emit errorReceived("Клиент не подключен к серверу");
        return;
    }

    // Сохраняем roomId для JOIN_ROOM
    if (request.find("JOIN_ROOM:") == 0) {
        std::stringstream ss(request.substr(10)); // Пропускаем "JOIN_ROOM:"
        std::string roomIdStr;
        std::getline(ss, roomIdStr, ':');
        try {
            pendingRoomId = std::stoi(roomIdStr);
            qDebug() << "Pending room ID set to:" << pendingRoomId;
        } catch (...) {
            qDebug() << "Invalid room ID in JOIN_ROOM";
        }
    }

    std::string data = request + "\n";
    qDebug() << "Sending request:" << request.c_str();
    int sent = send(socket_, data.c_str(), data.size(), 0);
    if (sent == SOCKET_ERROR) {
        qDebug() << "Send failed with error:" << WSAGetLastError();
        disconnect();
        emit errorReceived("Ошибка отправки запроса");
    }
}

void Client::sendFile(const std::string& filePath) {
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << filePath.c_str();
        return;
    }

    QByteArray fileData = file.readAll();
    QString fileName = QFileInfo(file).fileName();

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QDataStream ds(&buffer);
    ds.setVersion(QDataStream::Qt_5_15);

    int32_t magic = 0xFA57F11E;
    int32_t nameSize = fileName.size();
    int32_t dataSize = fileData.size();

    ds << magic << nameSize << dataSize;
    ds.writeRawData(fileName.toUtf8().constData(), nameSize);
    ds.writeRawData(fileData.constData(), dataSize);

    std::lock_guard<std::mutex> lock(mutex_);
    if (is_connected_) {
        qDebug() << "Sending file:" << fileName << "size:" << fileData.size();
        int sent = send(socket_, buffer.data().constData(), buffer.size(), 0);
        if (sent == SOCKET_ERROR) {
            qDebug() << "Send file failed with error:" << WSAGetLastError();
            disconnect();
            emit errorReceived("Ошибка отправки файла");
        }
    } else {
        qDebug() << "Cannot send file: not connected";
    }
}

void Client::receiveThread() {
    std::string readBuffer;
    char tempBuffer[4096];
    int bytesReceived;

    qDebug() << "Receive thread started";
    while (is_connected_) {
        bytesReceived = recv(socket_, tempBuffer, sizeof(tempBuffer), 0);
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            qDebug() << "Receive failed with error:" << error;
            disconnect();
            emit errorReceived("Ошибка получения данных: " + QString::number(error));
            break;
        }
        if (bytesReceived == 0) {
            qDebug() << "Connection closed by server";
            disconnect();
            emit errorReceived("Сервер закрыл соединение");
            break;
        }

        qDebug() << "Received" << bytesReceived << "bytes";
        readBuffer.append(tempBuffer, bytesReceived);

        if (readBuffer.size() >= sizeof(int32_t)) {
            int32_t magic;
            memcpy(&magic, readBuffer.data(), sizeof(int32_t));
            if (magic == static_cast<int32_t>(0xFA57F11E)) {
                qDebug() << "Processing file data";
                processFile(readBuffer);
                continue;
            }
        }

        size_t newlinePos;
        while ((newlinePos = readBuffer.find('\n')) != std::string::npos) {
            std::string line = readBuffer.substr(0, newlinePos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                qDebug() << "Processing response:" << QString::fromStdString(line);
                processResponse(line);
            }
            readBuffer.erase(0, newlinePos + 1);
        }
    }
    qDebug() << "Receive thread stopped";
}

void Client::processResponse(const std::string& response) {
    qDebug() << "Received response:" << QString::fromStdString(response);
    std::stringstream ss(response);
    std::string command;
    std::getline(ss, command, ':');

    if (command == "SUCCESS") {
        std::string action;
        std::getline(ss, action, ':');
        if (action == "Room created") {
            std::string roomIdStr;
            std::getline(ss, roomIdStr);
            try {
                int roomId = std::stoi(roomIdStr);
                currentRoomId = roomId;
                qDebug() << "Emitting roomCreated signal with ID:" << roomId;
                emit roomCreated(roomId);
            } catch (...) {
                qDebug() << "Invalid room ID in response";
                emit errorReceived("Некорректный ID комнаты");
            }
        } else if (action == "Joined room") {
            currentRoomId = pendingRoomId; // Устанавливаем roomId
            pendingRoomId = -1; // Сбрасываем
            qDebug() << "Emitting joinedRoom signal with room ID:" << currentRoomId;
            emit joinedRoom();
        } else if (action == "Message sent") {
            qDebug() << "Message sent confirmed";
        } else {
            qDebug() << "Unknown success action:" << QString::fromStdString(action);
            emit errorReceived("Неизвестное действие: " + QString::fromStdString(action));
        }
    } else if (command == "MESSAGE") {
        std::string roomIdStr, senderIdStr, message;
        std::getline(ss, roomIdStr, ':');
        std::getline(ss, senderIdStr, ':');
        std::getline(ss, message);
        try {
            int roomId = std::stoi(roomIdStr);
            int senderId = std::stoi(senderIdStr);
            qDebug() << "Emitting messageReceived for room" << roomId << "from sender" << senderId;
            emit messageReceived(roomId, senderId, QString::fromStdString(message));
        } catch (...) {
            qDebug() << "Invalid message format";
            emit errorReceived("Некорректный формат сообщения");
        }
    } else if (command == "MESSAGES") {
        std::string roomIdStr, messages;
        std::getline(ss, roomIdStr, ':');
        std::getline(ss, messages);
        try {
            int roomId = std::stoi(roomIdStr);
            if (messages != "None") {
                std::stringstream ms(messages);
                std::string messageEntry;
                while (std::getline(ms, messageEntry, ';')) {
                    std::stringstream me(messageEntry);
                    std::string senderIdStr, message;
                    std::getline(me, senderIdStr, ',');
                    std::getline(me, message);
                    int senderId = std::stoi(senderIdStr);
                    qDebug() << "Emitting messageReceived for room" << roomId << "from sender" << senderId;
                    emit messageReceived(roomId, senderId, QString::fromStdString(message));
                }
            } else {
                qDebug() << "No messages for room" << roomId;
            }
        } catch (...) {
            qDebug() << "Invalid messages format";
            emit errorReceived("Некорректный формат сообщений");
        }
    } else if (command == "ROOM_LIST") {
        std::string roomsStr;
        std::getline(ss, roomsStr);
        QStringList rooms;
        if (roomsStr != "None") {
            std::stringstream roomSs(roomsStr);
            std::string room;
            while (std::getline(roomSs, room, ';')) {
                rooms << QString::fromStdString(room);
            }
        }
        qDebug() << "Emitting roomListReceived with" << rooms.size() << "rooms";
        emit roomListReceived(rooms);
    } else if (command == "ERROR") {
        std::string error;
        std::getline(ss, error);
        qDebug() << "Emitting errorReceived:" << QString::fromStdString(error);
        emit errorReceived(QString::fromStdString(error));
    } else {
        qDebug() << "Unknown response:" << QString::fromStdString(response);
        emit errorReceived("Неизвестный ответ от сервера: " + QString::fromStdString(response));
    }
}

void Client::processFile(std::string& buffer) {
    QDataStream ds(QByteArray::fromRawData(buffer.data(), buffer.size()));
    ds.setVersion(QDataStream::Qt_5_15);

    int32_t magic, nameSize, dataSize;
    ds >> magic >> nameSize >> dataSize;

    if (buffer.size() < static_cast<size_t>(sizeof(int32_t) * 3 + nameSize + dataSize)) {
        qDebug() << "Incomplete file data, waiting for more";
        return;
    }

    QByteArray nameData, fileData;
    ds >> nameData >> fileData;

    qDebug() << "Emitting fileReceived for file:" << QString::fromUtf8(nameData);
    emit fileReceived(QString::fromUtf8(nameData), fileData);

    buffer.erase(0, sizeof(int32_t) * 3 + nameSize + dataSize);
}
