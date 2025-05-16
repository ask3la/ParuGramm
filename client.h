#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <thread>
#include <mutex>
#include <QObject>
#include <winsock2.h>
#include <ws2tcpip.h>

class Client;

class SingletonDestroyer {
private:
    Client* p_instance;
public:
    ~SingletonDestroyer();
    void initialize(Client* p);
};

class Client : public QObject {
    Q_OBJECT
public:
    static Client& getInstance();
    bool connectToServer(const std::string& ip, int port);
    void disconnect();
    void sendRequest(const std::string& request);
    void sendFile(const std::string& filePath);
    void setRoomId(int id) { currentRoomId = id; }
    int getRoomId() const { return currentRoomId; }

signals:
    void messageReceived(int roomId, int senderId, const QString& message);
    void roomCreated(int roomId);
    void joinedRoom();
    void roomListReceived(const QStringList& rooms);
    void errorReceived(const QString& error);
    void fileReceived(const QString& fileName, const QByteArray& fileData);

private:
    Client();
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    friend class SingletonDestroyer;

    static Client* p_instance;
    static SingletonDestroyer destroyer;

    void receiveThread();
    void processResponse(const std::string& response);
    void processFile(std::string& buffer);

    bool is_connected_;
    SOCKET socket_;
    std::mutex mutex_;
    std::thread receive_thread_;
    int currentRoomId = -1;
    int pendingRoomId = -1; // Для хранения roomId при JOIN_ROOM
};

#endif // CLIENT_H
