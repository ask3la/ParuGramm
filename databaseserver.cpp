#include "databaseserver.h"
#include "database.h"
#include <iostream>
#include <sstream>
#include <QSqlQuery>
#include <QDataStream>
#include <QVariant>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QtGlobal>
#include <QBuffer>

DatabaseServer* DatabaseServer::p_instance = nullptr;
SingletonDestroyer DatabaseServer::destroyer;

DatabaseServer& DatabaseServer::getInstance() {
    if (!p_instance) {
        p_instance = new DatabaseServer();
        destroyer.initialize(p_instance);
    }
    return *p_instance;
}

DatabaseServer::DatabaseServer() : running_(false), serverSocket_(INVALID_SOCKET), nextClientId_(1) {}

DatabaseServer::~DatabaseServer() {
    stop();
}

SingletonDestroyer::~SingletonDestroyer() {
    delete p_instance;
}

void SingletonDestroyer::initialize(DatabaseServer* p) {
    p_instance = p;
}

void DatabaseServer::start(int port) {
    qDebug() << "Server starting on port" << port;
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return;
    }

    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        closesocket(serverSocket_);
        WSACleanup();
        return;
    }

    if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(serverSocket_);
        WSACleanup();
        return;
    }

    running_ = true;
    std::thread(&DatabaseServer::handleConnections, this).detach();
}

void DatabaseServer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return;

    running_ = false;
    closesocket(serverSocket_);
    WSACleanup();
}

void DatabaseServer::handleConnections() {
    while (running_) {
        SOCKET clientSocket = accept(serverSocket_, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed.\n";
            continue;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[nextClientId_] = clientSocket;
        std::string response = "SUCCESS:Client ID:" + std::to_string(nextClientId_) + "\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
        qDebug() << "Assigned client ID:" << nextClientId_ << "to socket";
        std::thread(&DatabaseServer::handleClient, this, clientSocket).detach();
        nextClientId_++;
    }
}

void DatabaseServer::handleClient(SOCKET clientSocket) {
    int clientId = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, socket] : clients_) {
            if (socket == clientSocket) {
                clientId = id;
                break;
            }
        }
    }

    std::string readBuffer;
    char tempBuffer[65536];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, tempBuffer, sizeof(tempBuffer), 0)) > 0) {
        readBuffer.append(tempBuffer, bytesReceived);
        qDebug() << "Client" << clientId << "received" << bytesReceived << "bytes, buffer size:" << readBuffer.size();

        while (!readBuffer.empty()) {
            if (readBuffer.size() >= sizeof(uint32_t)) {
                QDataStream ds(QByteArray::fromRawData(readBuffer.data(), readBuffer.size()));
                ds.setVersion(QDataStream::Qt_5_15);
                ds.setByteOrder(QDataStream::LittleEndian);
                uint32_t magic;
                ds >> magic;
                if (magic == 0xFA57F11E) {
                    qDebug() << "Detected file packet for client" << clientId;
                    if (processFile(clientId, clientSocket, readBuffer)) {
                        continue; // Файл обработан, продолжаем
                    } else {
                        break; // Ждём больше данных
                    }
                } else {
                    qDebug() << "No file packet, magic:" << QString::number(magic, 16);
                }
            }

            size_t newlinePos = readBuffer.find('\n');
            if (newlinePos == std::string::npos) {
                if (readBuffer.size() > 10 * 1024 * 1024) { // Ограничение 10 МБ
                    qDebug() << "Buffer too large without newline, clearing";
                    readBuffer.clear();
                    std::string response = "ERROR:Buffer overflow\r\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                break;
            }

            std::string line = readBuffer.substr(0, newlinePos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                qDebug() << "Processing text command from client" << clientId << ":" << QString::fromStdString(line);
                std::string response = processRequest(clientId, clientSocket, line) + "\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            readBuffer.erase(0, newlinePos + 1);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(clientId);
        clientRooms_.erase(clientId);
    }
    qDebug() << "Client" << clientId << "disconnected";
    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
}

std::string DatabaseServer::processRequest(int clientId, SOCKET clientSocket, const std::string& request) {
    qDebug() << "Processing request from client" << clientId << ":" << QString::fromStdString(request);
    std::stringstream ss(request);
    std::string command;
    std::getline(ss, command, ':');

    if (command == "CREATE_ROOM") {
        std::string name, password;
        std::getline(ss, name, ':');
        std::getline(ss, password);
        return handleCreateRoom(name, password);
    } else if (command == "JOIN_ROOM") {
        std::string roomIdStr, password;
        std::getline(ss, roomIdStr, ':');
        std::getline(ss, password);
        int roomId;
        try {
            roomId = std::stoi(roomIdStr);
        } catch (...) {
            return "ERROR:Invalid room ID";
        }
        return handleJoinRoom(clientId, roomId, password);
    } else if (command == "MESSAGE") {
        std::string roomIdStr, message;
        std::getline(ss, roomIdStr, ':');
        std::getline(ss, message);
        int roomId;
        try {
            roomId = std::stoi(roomIdStr);
        } catch (...) {
            return "ERROR:Invalid room ID";
        }
        return handleMessage(clientId, roomId, message);
    } else if (command == "LIST_ROOMS") {
        return handleListRooms();
    } else if (command == "GET_MESSAGES") {
        std::string roomIdStr;
        std::getline(ss, roomIdStr);
        int roomId;
        try {
            roomId = std::stoi(roomIdStr);
        } catch (...) {
            return "ERROR:Invalid room ID";
        }
        QSqlQuery query(Database::instance().getDb());
        query.prepare("SELECT sender_id, message, file_path FROM messages WHERE room_id = ? ORDER BY timestamp");
        query.addBindValue(roomId);
        if (query.exec()) {
            std::string response = "MESSAGES:" + std::to_string(roomId) + ":";
            bool first = true;
            while (query.next()) {
                if (!first) response += ";";
                int senderId = query.value(0).toInt();
                std::string message = query.value(1).toString().toStdString();
                std::string filePath = query.value(2).toString().toStdString();
                response += std::to_string(senderId) + "," + (filePath.empty() ? message : "File: " + filePath);
                first = false;
            }
            return response.empty() ? "MESSAGES:" + std::to_string(roomId) + ":None" : response;
        } else {
            return "ERROR:Failed to fetch messages:" + query.lastError().text().toStdString();
        }
    } else if (command == "GET_USERS") {
        std::string roomIdStr;
        std::getline(ss, roomIdStr);
        int roomId;
        try {
            roomId = std::stoi(roomIdStr);
        } catch (...) {
            return "ERROR:Invalid room ID";
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::string response = "USER_LIST:";
        bool first = true;
        for (const auto& [id, socket] : clients_) {
            if (clientRooms_.find(id) != clientRooms_.end() && clientRooms_[id] == roomId) {
                if (!first) response += ";";
                response += std::to_string(id);
                first = false;
            }
        }
        return response.empty() ? "USER_LIST:None" : response;
    } else if (command == "LEAVE_ROOM") {
        std::string roomIdStr;
        std::getline(ss, roomIdStr);
        int roomId;
        try {
            roomId = std::stoi(roomIdStr);
        } catch (...) {
            return "ERROR:Invalid room ID";
        }
        return handleLeaveRoom(clientId);
    }
    else {
        qDebug() << "Unknown command from client" << clientId << ":" << QString::fromStdString(command);
        return "ERROR:Unknown command";
    }
}

std::string DatabaseServer::handleCreateRoom(const std::string& name, const std::string& password) {
    QSqlQuery query(Database::instance().getDb());
    query.prepare("INSERT INTO rooms (name, password) VALUES (:name, :password)");
    query.bindValue(":name", QVariant(QString::fromStdString(name)));
    query.bindValue(":password", QVariant(QString::fromStdString(password)));
    if (query.exec()) {
        int roomId = query.lastInsertId().toInt();
        return "SUCCESS:Room created:" + std::to_string(roomId);
    } else {
        return "ERROR:Failed to create room:" + query.lastError().text().toStdString();
    }
}

std::string DatabaseServer::handleJoinRoom(int clientId, int roomId, const std::string& password) {
    QSqlQuery query(Database::instance().getDb());
    query.prepare("SELECT id, name, password FROM rooms WHERE id = ?");
    query.addBindValue(roomId);

    if (!query.exec() || !query.next()) {
        return "ERROR:Room not found";
    }

    QString dbPassword = query.value("password").toString();
    if (!dbPassword.isEmpty() && dbPassword != QString::fromStdString(password)) {
        return "ERROR:Invalid password";
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clientRooms_.count(clientId)) {
            int oldRoomId = clientRooms_[clientId];
            clientRooms_.erase(clientId);
            notifyUserLeft(oldRoomId, clientId);
        }
        clientRooms_[clientId] = roomId;
    }

    // Отправляем сначала подтверждение входа
    std::string response = "SUCCESS:Joined room\r\n";
    send(clients_[clientId], response.c_str(), response.size(), 0);

    // Затем отправляем список пользователей
    std::string userList;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool first = true;
        for (const auto& [id, rId] : clientRooms_) {
            if (rId == roomId) {
                if (!first) userList += ";";
                userList += std::to_string(id);
                first = false;
            }
        }
        if (first) userList = "None";
    }

    std::string userListResponse = "USER_LIST:" + userList + "\r\n";
    send(clients_[clientId], userListResponse.c_str(), userListResponse.size(), 0);

    notifyUserJoined(roomId, clientId);
    return ""; // Мы уже отправили ответы вручную
}

std::string DatabaseServer::handleMessage(int clientId, int roomId, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clientRooms_.find(clientId) == clientRooms_.end() || clientRooms_[clientId] != roomId) {
            return "ERROR:Not in room";
        }
    }

    QSqlQuery query(Database::instance().getDb());
    query.prepare("INSERT INTO messages (room_id, sender_id, message) VALUES (:roomId, :senderId, :message)");
    query.bindValue(":roomId", QVariant(roomId));
    query.bindValue(":senderId", QVariant(clientId));
    query.bindValue(":message", QVariant(QString::fromStdString(message)));
    if (query.exec()) {
        broadcastMessage(roomId, clientId, message);
        return "SUCCESS:Message sent";
    } else {
        return "ERROR:Failed to save message:" + query.lastError().text().toStdString();
    }
}

std::string DatabaseServer::handleListRooms() {
    QSqlQuery query(Database::instance().getDb());
    // Получаем список всех комнат с их названиями и наличием пароля
    query.prepare("SELECT id, name, password FROM rooms");
    if (!query.exec()) {
        qDebug() << "Failed to fetch rooms:" << query.lastError().text();
        return "ERROR:Failed to fetch room list";
    }

    // Собираем статистику по участникам в комнатах
    std::map<int, int> userCounts;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [clientId, roomId] : clientRooms_) {
            userCounts[roomId]++;
        }
    }

    std::string result = "ROOM_LIST:";
    bool first = true;
    while (query.next()) {
        if (!first) result += ";";

        int roomId = query.value("id").toInt();
        QString name = query.value("name").toString();
        QString password = query.value("password").toString();
        int participants = userCounts[roomId];

        result += QString("%1:%2:%3:%4")
                      .arg(roomId)
                      .arg(name)
                      .arg(password.isEmpty() ? "No" : "Yes")
                      .arg(participants)
                      .toStdString();

        first = false;
    }
    return result.empty() ? "ROOM_LIST:None" : result;
}

void DatabaseServer::broadcastMessage(int roomId, int senderId, const std::string& message) {
    std::string response = "MESSAGE:" + std::to_string(roomId) + ":" + std::to_string(senderId) + ":" + message + "\r\n";
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, socket] : clients_) {
        if (clientRooms_.find(id) != clientRooms_.end() && clientRooms_[id] == roomId) {
            send(socket, response.c_str(), response.size(), 0);
        }
    }
}

bool DatabaseServer::processFile(int clientId, SOCKET clientSocket, std::string& buffer) {
    if (buffer.size() < 3 * sizeof(int32_t)) {
        qDebug() << "Not enough data for file header, need" << (3 * sizeof(int32_t)) << ", have" << buffer.size();
        return false;
    }

    QDataStream ds(QByteArray::fromRawData(buffer.data(), buffer.size()));
    ds.setVersion(QDataStream::Qt_5_15);
    ds.setByteOrder(QDataStream::LittleEndian);

    int32_t magic, nameSize, dataSize;
    ds >> magic >> nameSize >> dataSize;

    if (magic != 0xFA57F11E) {
        qDebug() << "Invalid magic number:" << QString::number(magic, 16);
        buffer.clear();
        std::string response = "ERROR:Invalid file packet\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
        return true;
    }

    if (nameSize <= 0 || nameSize > 1024 || dataSize < 0 || dataSize > 10 * 1024 * 1024) {
        qDebug() << "Invalid file packet: nameSize=" << nameSize << ", dataSize=" << dataSize;
        buffer.clear();
        std::string response = "ERROR:Invalid file packet\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
        return true;
    }

    size_t totalSize = sizeof(int32_t) * 3 + nameSize + dataSize;
    if (buffer.size() < totalSize) {
        qDebug() << "Incomplete file data for client" << clientId << ", need" << totalSize << ", have" << buffer.size();
        return false; // Ждём больше данных
    }

    QByteArray nameData = QByteArray(buffer.data() + 12, nameSize);
    QByteArray fileData = QByteArray(buffer.data() + 12 + nameSize, dataSize);
    qDebug() << "nameData (hex):" << nameData.toHex();
    qDebug() << "fileData size:" << fileData.size();

    QString fileName = QString::fromUtf8(nameData);
    qDebug() << "Parsed fileName:" << fileName;
    if (fileName.isEmpty()) {
        qDebug() << "Invalid file name (empty after UTF-8 conversion)";
        buffer.erase(0, totalSize);
        std::string response = "ERROR:Invalid file name\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
        return true;
    }

    qDebug() << "Processing file from client" << clientId << ":" << fileName << ", size:" << dataSize;

    int roomId = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clientRooms_.find(clientId) != clientRooms_.end()) {
            roomId = clientRooms_[clientId];
        }
    }

    if (roomId != -1) {
        saveFile(roomId, clientId, fileName, fileData);
        std::string fileBuffer = buffer.substr(0, totalSize);
        broadcastFile(roomId, clientId, fileBuffer);
        std::string response = "SUCCESS:File received\r\n";
        qDebug() << "Sent response bytes:" << send(clientSocket, response.c_str(), response.size(), 0);
    } else {
        qDebug() << "Client" << clientId << "not in any room, ignoring file";
        std::string response = "ERROR:Not in room\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
    }

    buffer.erase(0, totalSize);
    qDebug() << "File processed, remaining buffer size:" << buffer.size();
    return true;
}

void DatabaseServer::saveFile(int roomId, int senderId, const QString& fileName, const QByteArray& fileData) {
    QDir().mkpath("uploads");
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString uniqueFileName = QString("uploads/%1_%2_%3").arg(senderId).arg(timestamp).arg(fileName);

    QFile file(uniqueFileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(fileData);
        file.close();
        qDebug() << "File saved:" << uniqueFileName;

        QSqlQuery query(Database::instance().getDb());
        query.prepare("INSERT INTO messages (room_id, sender_id, message, file_path) "
                      "VALUES (:roomId, :senderId, :message, :filePath)");
        query.bindValue(":roomId", roomId);
        query.bindValue(":senderId", senderId);
        query.bindValue(":message", QString("File: %1").arg(fileName));
        query.bindValue(":filePath", uniqueFileName);
        if (!query.exec()) {
            qDebug() << "Error saving file metadata:" << query.lastError().text();
        } else {
            qDebug() << "File metadata saved for" << fileName;
        }
    } else {
        qDebug() << "Failed to save file:" << uniqueFileName;
    }
}

void DatabaseServer::broadcastFile(int roomId, int senderId, const std::string& buffer) {
    QBuffer newBuffer;
    newBuffer.open(QIODevice::WriteOnly);
    QDataStream ds(&newBuffer);
    ds.setVersion(QDataStream::Qt_5_15);
    ds.setByteOrder(QDataStream::LittleEndian);

    QByteArray originalData = QByteArray::fromRawData(buffer.data(), buffer.size());
    QDataStream origDs(originalData);
    origDs.setVersion(QDataStream::Qt_5_15);
    origDs.setByteOrder(QDataStream::LittleEndian);

    int32_t magic, nameSize, dataSize;
    origDs >> magic >> nameSize >> dataSize;

    ds << magic << senderId << nameSize << dataSize;
    newBuffer.write(originalData.mid(12));

    QByteArray packet = newBuffer.data();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, socket] : clients_) {
        if (clientRooms_.find(id) != clientRooms_.end() && clientRooms_[id] == roomId && id != senderId) {
            int totalSent = 0;
            while (totalSent < packet.size()) {
                int sent = send(socket, packet.data() + totalSent, packet.size() - totalSent, 0);
                if (sent == SOCKET_ERROR) {
                    qDebug() << "Broadcast file failed for client" << id << "with error:" << WSAGetLastError();
                    break;
                }
                totalSent += sent;
            }
            qDebug() << "Broadcasted file to client" << id << "from sender" << senderId << ", sent:" << totalSent;
        }
    }
}

std::string DatabaseServer::handleLeaveRoom(int clientId) {
    int roomId = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clientRooms_.count(clientId)) {
            roomId = clientRooms_[clientId];
            clientRooms_.erase(clientId);
        }
    }

    if (roomId != -1) {
        notifyUserLeft(roomId, clientId);
        return "SUCCESS:Left room";
    }
    return "ERROR:Not in any room";
}

void DatabaseServer::notifyUserJoined(int roomId, int userId) {
    std::string response = "USER_JOINED:" + std::to_string(roomId) + ":" + std::to_string(userId) + "\r\n";
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, socket] : clients_) {
        if (clientRooms_.find(id) != clientRooms_.end() && clientRooms_[id] == roomId) {
            send(socket, response.c_str(), response.size(), 0);
        }
    }
}

void DatabaseServer::notifyUserLeft(int roomId, int userId) {
    std::string response = "USER_LEFT:" + std::to_string(roomId) + ":" + std::to_string(userId) + "\r\n";
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, socket] : clients_) {
        if (clientRooms_.find(id) != clientRooms_.end() && clientRooms_[id] == roomId) {
            send(socket, response.c_str(), response.size(), 0);
        }
    }
}

