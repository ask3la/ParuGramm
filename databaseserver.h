#ifndef DATABASESERVER_H
#define DATABASESERVER_H

#include <winsock2.h>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "database.h"

class DatabaseServer;

class SingletonDestroyer {
public:
    SingletonDestroyer() = default;
    ~SingletonDestroyer();
    void initialize(DatabaseServer* p);

private:
    DatabaseServer* p_instance = nullptr;
};

class DatabaseServer {
public:
    static DatabaseServer& getInstance();
    void start(int port);
    void stop();
    ~DatabaseServer();

private:
    DatabaseServer();
    friend class SingletonDestroyer;
    static DatabaseServer* p_instance;
    static SingletonDestroyer destroyer;

    bool running_;
    SOCKET serverSocket_;
    std::map<int, SOCKET> clients_;
    std::map<int, int> clientRooms_;
    std::mutex mutex_;
    int nextClientId_;
    void handleConnections();
    void handleClient(SOCKET clientSocket);
    std::string processRequest(int clientId, SOCKET clientSocket, const std::string& request);
    std::string handleCreateRoom(const std::string& name, const std::string& password);
    std::string handleJoinRoom(int clientId, int roomId, const std::string& password);
    std::string handleMessage(int clientId, int roomId, const std::string& message);
    std::string handleListRooms();
    void broadcastMessage(int roomId, int senderId, const std::string& message);
    bool processFile(int clientId, SOCKET clientSocket, std::string& buffer); // Изменено на bool
    void saveFile(int roomId, int senderId, const QString& fileName, const QByteArray& fileData);
    void broadcastFile(int roomId, int senderId, const std::string& buffer);

    void notifyUserJoined(int roomId, int userId);
    void notifyUserLeft(int roomId, int userId);
    std::string handleLeaveRoom(int clientId);

    // Добавляем в приватные поля класса
    std::map<int, int> roomUserCounts_; // roomId -> user count
    std::mutex roomCountsMutex_;
};

#endif // DATABASESERVER_H
