#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include "client.h"
#include <QWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QMap>

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    ChatWindow(int roomId, Client *client, QWidget *parent = nullptr);

private slots:
    void sendMessage();
    void selectEmoji();
    void sendFile();
    void handleFileSent(const QString& fileName, const QByteArray& fileData);
    void handleAnchorClicked(const QUrl& url);

    void onLeaveRoomClicked();
    void handleUserJoined(int roomId, int userId);
    void handleUserLeft(int roomId, int userId);
    void handleLeftRoom();

private:
    void setupConnections();
    void updateUserList(const QStringList& users);
    QColor getUserColor(int userId) const;
    QString getUserName(int userId) const;

    int roomId;
    Client *client;
    QTextBrowser *chatArea;
    QListWidget *userList;
    QLabel *userListLabel;
    QLineEdit *messageInput;
    QPushButton *sendButton;
    QPushButton *emojiButton;
    QPushButton *fileButton;
    QPushButton *leaveButton;
    QLabel *logoLabel;
    QLabel *titleLabel;
    struct FileData {
        QString fileName;
        QByteArray data;
    };
    QMap<QString, FileData> fileDataMap;
    int lastSenderId;
    bool lastWasEvent;
    QStringList lastUserList;
};

#endif // CHATWINDOW_H
