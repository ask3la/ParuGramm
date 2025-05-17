#include "client.h"
#include <iostream>
#include <sstream>
#include <QDebug>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QDateTime>
#include <QtGlobal>
#include <QRegularExpression>
#include <vector>

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

Client::Client() : is_connected_(false), socket_(INVALID_SOCKET), currentRoomId(-1), pendingRoomId(-1), clientId(-1) {
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

    is_connected_ = false;
    if (socket_ != INVALID_SOCKET) {
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (receive_thread_.joinable()) {
        receive_thread_.join(); // БЛОКИРУЕМСЯ пока поток не завершится!
    }
    clientId = -1;
    currentRoomId = -1;
    pendingRoomId = -1;
}

void Client::sendRequest(const std::string& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_connected_) {
        qDebug() << "Cannot send request: not connected";
        emit errorReceived("Клиент не подключен к серверу");
        return;
    }

    if (request.find("JOIN_ROOM:") == 0) {
        std::stringstream ss(request.substr(10));
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
        emit errorReceived("Не удалось открыть файл");
        return;
    }

    QByteArray fileData = file.readAll();
    if (fileData.size() > 10 * 1024 * 1024) { // Ограничение 10 МБ
        qDebug() << "File too large:" << filePath.c_str();
        emit errorReceived("Файл слишком большой (макс. 10 МБ)");
        return;
    }

    QString fileName = QFileInfo(file).fileName();
    if (fileName.isEmpty()) {
        qDebug() << "Invalid file name (empty):" << fileName;
        emit errorReceived("Недопустимое имя файла");
        return;
    }

    QByteArray fileNameUtf8 = fileName.toUtf8();

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QDataStream ds(&buffer);
    ds.setVersion(QDataStream::Qt_5_15);
    ds.setByteOrder(QDataStream::LittleEndian);

    uint32_t magic = 0xFA57F11E;
    int32_t nameSize = fileNameUtf8.size();
    int32_t dataSize = fileData.size();

    ds << magic << nameSize << dataSize;
    buffer.write(fileNameUtf8);
    buffer.write(fileData);

    QByteArray packet = buffer.data();
    qDebug() << "Packet size:" << packet.size();
    qDebug() << "Packet header (hex):" << packet.left(12).toHex();
    qDebug() << "nameSize:" << nameSize << ", dataSize:" << dataSize;
    qDebug() << "fileNameUtf8 (hex):" << fileNameUtf8.toHex();

    std::lock_guard<std::mutex> lock(mutex_);
    if (is_connected_) {
        qDebug() << "Sending file:" << fileName << "size:" << fileData.size();
        int totalSent = 0;
        while (totalSent < packet.size()) {
            int sent = send(socket_, packet.constData() + totalSent, packet.size() - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                qDebug() << "Send file failed with error:" << WSAGetLastError();
                disconnect();
                emit errorReceived("Ошибка отправки файла");
                break;
            }
            totalSent += sent;
        }
        if (totalSent == packet.size()) {
            emit fileSent(fileName, fileData);
        }
    } else {
        qDebug() << "Cannot send file: not connected";
        emit errorReceived("Клиент не подключен к серверу");
    }
}

void Client::receiveThread() {
    std::string readBuffer;
    char tempBuffer[65536];
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

        if (readBuffer.size() >= sizeof(uint32_t)) {
            QDataStream ds(QByteArray::fromRawData(readBuffer.data(), readBuffer.size()));
            ds.setVersion(QDataStream::Qt_5_15);
            ds.setByteOrder(QDataStream::LittleEndian);
            uint32_t magic;
            ds >> magic;
            if (magic == 0xFA57F11E) {
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
    qDebug() << "Received raw response:" << QString::fromStdString(response);

    // Разделяем ответ на отдельные строки
    std::vector<std::string> lines;
    size_t start = 0;
    size_t end = response.find('\n');

    while (end != std::string::npos) {
        lines.push_back(response.substr(start, end - start));
        start = end + 1;
        end = response.find('\n', start);
    }
    lines.push_back(response.substr(start));

    // Обрабатываем каждую строку
    for (const auto& line : lines) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        std::getline(ss, command, ':');
        qDebug() << "Processing command:" << QString::fromStdString(command);

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
            }
            else if (action == "Joined room") {
                currentRoomId = pendingRoomId;
                pendingRoomId = -1;
                qDebug() << "Emitting joinedRoom signal with room ID:" << currentRoomId;
                emit joinedRoom();

                // После входа в комнату запросим список пользователей
                if (currentRoomId != -1) {
                    sendRequest("GET_USERS:" + std::to_string(currentRoomId));
                }
            }
            else if (action == "Left room") {
                qDebug() << "Left room confirmed by server";
                emit leftRoom();
            }
            else if (action == "Message sent") {
                qDebug() << "Message sent confirmed";
            }
            else if (action == "File received") {
                qDebug() << "File received confirmed by server";
                emit fileSentConfirmed();
            }
            else if (action == "Client ID") {
                std::string clientIdStr;
                std::getline(ss, clientIdStr);
                try {
                    clientId = std::stoi(clientIdStr);
                    qDebug() << "Assigned client ID:" << clientId;
                } catch (...) {
                    qDebug() << "Invalid client ID in response";
                    emit errorReceived("Некорректный ID клиента");
                }
            }
            else {
                qDebug() << "Unknown success action:" << QString::fromStdString(action);
                emit errorReceived("Неизвестное действие: " + QString::fromStdString(action));
            }
        }
        else if (command == "MESSAGE") {
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
        }
        else if (command == "MESSAGES") {
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
        }
        else if (command == "ROOM_LIST") {
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
        }
        else if (command == "USER_LIST") {
            std::string usersStr;
            std::getline(ss, usersStr);
            QStringList users;
            if (usersStr != "None") {
                std::stringstream userSs(usersStr);
                std::string user;
                while (std::getline(userSs, user, ';')) {
                    users << "User" + QString::fromStdString(user);
                }
            }
            qDebug() << "Emitting usersReceived with" << users.size() << "users";
            emit usersReceived(users);
        }
        else if (command == "ERROR") {
            std::string error;
            std::getline(ss, error);
            qDebug() << "Emitting errorReceived:" << QString::fromStdString(error);
            emit errorReceived(QString::fromStdString(error));
        }
        else if (command == "USER_JOINED") {
            std::string roomIdStr, userIdStr;
            std::getline(ss, roomIdStr, ':');
            std::getline(ss, userIdStr);
            try {
                int roomId = std::stoi(roomIdStr);
                int userId = std::stoi(userIdStr);
                emit userJoined(roomId, userId);
            } catch (...) {
                qDebug() << "Invalid USER_JOINED format";
            }
        }
        else if (command == "USER_LEFT") {
            std::string roomIdStr, userIdStr;
            std::getline(ss, roomIdStr, ':');
            std::getline(ss, userIdStr);
            try {
                int roomId = std::stoi(roomIdStr);
                int userId = std::stoi(userIdStr);
                emit userLeft(roomId, userId);
            } catch (...) {
                qDebug() << "Invalid USER_LEFT format";
            }
        }
        else {
            qDebug() << "Unknown response:" << QString::fromStdString(line);
            emit errorReceived("Неизвестный ответ от сервера: " + QString::fromStdString(line));
        }
    }
}

void Client::processFile(std::string& buffer) {
    QDataStream ds(QByteArray::fromRawData(buffer.data(), buffer.size()));
    ds.setVersion(QDataStream::Qt_5_15);
    ds.setByteOrder(QDataStream::LittleEndian);

    uint32_t magic, nameSize, dataSize;
    int32_t senderId;
    ds >> magic >> senderId >> nameSize >> dataSize;

    if (buffer.size() < static_cast<size_t>(sizeof(uint32_t) * 3 + sizeof(int32_t) + nameSize + dataSize)) {
        qDebug() << "Incomplete file data, waiting for more";
        return;
    }

    QByteArray nameData = QByteArray(buffer.data() + 16, nameSize);
    QByteArray fileData = QByteArray(buffer.data() + 16 + nameSize, dataSize);
    qDebug() << "Received nameData (hex):" << nameData.toHex();
    qDebug() << "Received fileData size:" << fileData.size();
    qDebug() << "Sender ID:" << senderId;

    QString fileName = QString::fromUtf8(nameData);
    qDebug() << "Parsed fileName:" << fileName;

    qDebug() << "Emitting fileReceived for file:" << fileName;
    emit fileReceived(currentRoomId, senderId, fileName, fileData);

    buffer.erase(0, sizeof(uint32_t) * 3 + sizeof(int32_t) + nameSize + dataSize);
}

// Важно: leaveRoom НЕ вызывает disconnect()
void Client::leaveRoom() {
    if (currentRoomId != -1) {
        sendRequest("LEAVE_ROOM:" + std::to_string(currentRoomId));
        currentRoomId = -1;
        // disconnect(); // НЕ вызываем здесь!
    }
}
