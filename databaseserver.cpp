#include "databaseserver.h"
#include "database.h"
#include <iostream>
#include <sstream>
#include <QSqlQuery>
#include <QDataStream>
#include <QVariant>
#include <QDebug>

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
    char tempBuffer[4096];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, tempBuffer, sizeof(tempBuffer), 0)) > 0) {
        readBuffer.append(tempBuffer, bytesReceived);

        // Проверяем, является ли это файлом
        if (readBuffer.size() >= sizeof(int32_t)) {
            int32_t magic;
            memcpy(&magic, readBuffer.data(), sizeof(int32_t));
            if (magic == static_cast<int32_t>(0xFA57F11E)) { // FILE_MAGIC
                processFile(clientId, clientSocket, readBuffer);
                continue;
            }
        }

        // Обрабатываем текстовые сообщения
        size_t newlinePos;
        while ((newlinePos = readBuffer.find('\n')) != std::string::npos) {
            std::string line = readBuffer.substr(0, newlinePos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
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
    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
}

std::string DatabaseServer::processRequest(int clientId, SOCKET clientSocket, const std::string& request) {
    // Убрали лишний qDebug
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
        query.prepare("SELECT sender_id, message FROM messages WHERE room_id = ? ORDER BY timestamp");
        query.addBindValue(roomId);
        if (query.exec()) {
            std::string response = "MESSAGES:" + std::to_string(roomId) + ":";
            bool first = true;
            while (query.next()) {
                if (!first) response += ";";
                int senderId = query.value(0).toInt();
                std::string message = query.value(1).toString().toStdString();
                response += std::to_string(senderId) + "," + message;
                first = false;
            }
            return response.empty() ? "MESSAGES:" + std::to_string(roomId) + ":None" : response;
        } else {
            return "ERROR:Failed to fetch messages:" + query.lastError().text().toStdString();
        }
    } else {
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
    query.prepare("SELECT password FROM rooms WHERE id = :roomId");
    query.bindValue(":roomId", QVariant(roomId));
    if (!query.exec() || !query.next()) {
        return "ERROR:Room not found";
    }

    std::string storedPassword = query.value("password").toString().toStdString();
    if (storedPassword.empty() || storedPassword == password) {
        std::lock_guard<std::mutex> lock(mutex_);
        clientRooms_[clientId] = roomId;
        return "SUCCESS:Joined room";
    } else {
        return "ERROR:Invalid password";
    }
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
    if (!query.exec()) {
        return "ERROR:Failed to save message:" + query.lastError().text().toStdString();
    }

    broadcastMessage(roomId, clientId, message);
    return "SUCCESS:Message sent";
}

std::string DatabaseServer::handleListRooms() {
    QSqlQuery query(Database::instance().getDb());
    query.exec("SELECT id, name FROM rooms");
    std::string result = "ROOM_LIST:";
    bool first = true;
    while (query.next()) {
        if (!first) result += ";";
        result += query.value("id").toString().toStdString() + ":" + query.value("name").toString().toStdString();
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

void DatabaseServer::processFile(int clientId, SOCKET clientSocket, std::string& buffer) {
    QDataStream ds(QByteArray::fromRawData(buffer.data(), buffer.size()));
    ds.setVersion(QDataStream::Qt_5_15);

    int32_t magic, nameSize, dataSize;
    ds >> magic >> nameSize >> dataSize;

    if (buffer.size() < static_cast<size_t>(sizeof(int32_t) * 3 + nameSize + dataSize)) {
        return; // Ждем больше данных
    }

    QByteArray nameData, fileData;
    ds >> nameData >> fileData;

    int roomId = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clientRooms_.find(clientId) != clientRooms_.end()) {
            roomId = clientRooms_[clientId];
        }
    }

    if (roomId != -1) {
        broadcastFile(roomId, clientId, buffer);
    }

    // Удаляем обработанные данные
    buffer.erase(0, sizeof(int32_t) * 3 + nameSize + dataSize);
}

void DatabaseServer::broadcastFile(int roomId, int senderId, const std::string& buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, socket] : clients_) {
        if (clientRooms_.find(id) != clientRooms_.end() && clientRooms_[id] == roomId) {
            send(socket, buffer.data(), buffer.size(), 0);
        }
    }
}
