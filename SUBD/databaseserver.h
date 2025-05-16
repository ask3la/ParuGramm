#ifndef DATABASESERVER_H
#define DATABASESERVER_H

#include <string>
#include <mutex>
#include <thread>
#include <map>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

class DatabaseServer;

class SingletonDestroyer {
private:
    DatabaseServer* p_instance;
public:
    ~SingletonDestroyer();
    void initialize(DatabaseServer* p);
};

class DatabaseServer {
public:
    static DatabaseServer& getInstance();

    void start(int port);
    void stop();

private:
    DatabaseServer();
    ~DatabaseServer();

    DatabaseServer(const DatabaseServer&) = delete;
    DatabaseServer& operator=(const DatabaseServer&) = delete;

    friend class SingletonDestroyer;

    static DatabaseServer* p_instance;
    static SingletonDestroyer destroyer;

    void handleConnections();
    void handleClient(SOCKET clientSocket);
    std::string processRequest(int clientId, SOCKET clientSocket, const std::string& request);
    void processFile(int clientId, SOCKET clientSocket, std::string& buffer);
    std::string handleCreateRoom(const std::string& name, const std::string& password);
    std::string handleJoinRoom(int clientId, int roomId, const std::string& password);
    std::string handleMessage(int clientId, int roomId, const std::string& message);
    std::string handleListRooms();
    void broadcastMessage(int roomId, int senderId, const std::string& message);
    void broadcastFile(int roomId, int senderId, const std::string& buffer);

    bool running_;
    SOCKET serverSocket_;
    std::mutex mutex_;
    std::map<int, SOCKET> clients_; // client_id -> socket
    std::map<int, int> clientRooms_; // client_id -> room_id
    int nextClientId_;
};

#endif // DATABASESERVER_H
