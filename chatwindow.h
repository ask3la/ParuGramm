#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include "client.h"

class EmojiPanel;

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    ChatWindow(int roomId, Client *client, QWidget *parent = nullptr);
    ~ChatWindow() = default;

private:
    void setupConnections();
    QTextEdit *chatArea;
    QListWidget *userList;
    QLineEdit *messageInput;
    QPushButton *sendButton;
    QPushButton *emojiButton;
    QPushButton *fileButton;
    int roomId;
    Client *client;

private slots:
    void sendMessage();
    void selectEmoji();
    void sendFile();
};

#endif // CHATWINDOW_H
