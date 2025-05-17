#include "chatwindow.h"
#include "emojipanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QPushButton>
#include <QDebug>
#include <QLabel>
#include <QScreen>
#include <QUrl>
#include <QTextBrowser>
#include <QTextCursor>
#include <QScrollBar>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <mainwindow.h>
#include <QRegularExpression>

static QStringList colorList = {
    "#90EE90", // light green
    "#87CEEB", // sky blue
    "#FFA500", // orange
    "#FF69B4", // pink
    "#DA70D6", // orchid
    "#00CED1"  // turquoise
};

ChatWindow::ChatWindow(int roomId, Client *client, QWidget *parent)
    : QWidget(parent), roomId(roomId), client(client), lastSenderId(-1), lastWasEvent(false)
{
    setWindowTitle("Чат: Комната " + QString::number(roomId));
    setFixedSize(800, 875);
    setWindowIcon(QIcon(":/icons/snake.png"));

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Header: Logo, Title, Leave Button ---
    QHBoxLayout *headerLayout = new QHBoxLayout();
    logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/icons/snake.png");
    logoLabel->setPixmap(logoPixmap.scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    titleLabel = new QLabel("ParuGramm Chat", this);
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #27ec2f; margin-left: 12px;");
    leaveButton = new QPushButton("Выйти", this);
    leaveButton->setFixedSize(100, 40);
    leaveButton->setStyleSheet(
        "QPushButton { background-color: #d5394e; color: #FFFFFF; border: 1px solid #E0E0E0; border-radius: 5px; padding: 8px; font-weight: bold; font-size: 16px; }"
        "QPushButton:hover { background-color: #a5182a; border: 2px solid #E0E0E0; }"
        "QPushButton:pressed { background-color: #8b1423; }"
        );
    headerLayout->addWidget(logoLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(leaveButton);
    mainLayout->addLayout(headerLayout);

    // --- User List Section ---
    userListLabel = new QLabel(this);
    userListLabel->setStyleSheet("font-size: 18px; color: #AFFF99; font-weight: bold; margin-bottom: 2px; margin-left: 3px;");
    userList = new QListWidget(this);
    userList->setMinimumHeight(105);
    userList->setMaximumHeight(120);
    userList->setStyleSheet(
        "QListWidget { background-color: #263238; border: 1px solid #4CAF50; border-radius: 7px; font-size: 16px; padding-left: 5px; color: #E0E0E0; }"
        "QListWidget::item { padding: 7px 0 7px 10px; border-radius: 5px; height: 32px; }"
        "QListWidget::item:selected { background: #4CAF50; color: #E0E0E0; border-radius: 3px; }"
        );
    mainLayout->addSpacing(2);
    mainLayout->addWidget(userListLabel);
    mainLayout->addWidget(userList);

    // --- Chat Area ---
    chatArea = new QTextBrowser(this);
    chatArea->setReadOnly(true);
    chatArea->setMinimumHeight(400);
    chatArea->setStyleSheet("font-size: 20px; background-color: #1A2526; border: none; padding: 10px;");
    chatArea->setOpenExternalLinks(false);
    chatArea->setOpenLinks(false);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(chatArea);

    // --- Message Input/Buttons ---
    QHBoxLayout *inputLayout = new QHBoxLayout();
    messageInput = new QLineEdit(this);
    messageInput->setMinimumHeight(40);
    messageInput->setPlaceholderText("Написать сообщение...");
    messageInput->setStyleSheet(
        "QLineEdit { background-color: #1e292e; border: 1px solid #4CAF50; border-radius: 5px; padding: 5px; font-size: 16px; color: #E0E0E0; }"
        "QLineEdit:placeholder { color: #A0A0A0; }"
        );
    sendButton = new QPushButton("Отправить", this);
    sendButton->setFixedSize(125, 40);
    emojiButton = new QPushButton("😊", this);
    emojiButton->setFixedSize(50, 40);
    fileButton = new QPushButton("📎", this);
    fileButton->setFixedSize(50, 40);

    inputLayout->addWidget(messageInput);
    inputLayout->addWidget(sendButton);
    inputLayout->addWidget(emojiButton);
    inputLayout->addWidget(fileButton);

    mainLayout->addLayout(inputLayout);

    setLayout(mainLayout);
    setupConnections();


    setStyleSheet(R"(
        QWidget {
            background-color: #1A2526;
            color: #E0E0E0;
        }
        QLineEdit {
            background-color: #263238;
            border: 1px solid #4CAF50;
            border-radius: 5px;
            padding: 5px;
            font-size: 16px;
            color: #E0E0E0;
        }
        QPushButton {
            background-color: #4CAF50;
            color: #E0E0E0;
            border: 1px solid #E0E0E0;
            border-radius: 5px;
            padding: 8px;
            font-weight: bold;
            font-size: 16px;
        }
        QPushButton:hover {
            background-color: #388E3C;
            border: 2px solid #E0E0E0;
        }
        QPushButton:pressed {
            background-color: #2E7D32;
        }
    )");

    qDebug() << "ChatWindow created for room ID:" << roomId;
    show();


    if (client && roomId != -1) {   //хз можно убрать удмаю
        client->sendRequest("GET_USERS:" + std::to_string(roomId));
    }
}

QColor ChatWindow::getUserColor(int userId) const {
    if (userId == client->getClientId()) {
        qDebug() << "getUserColor: userId =" << userId << ", returning #FFFFFF (client)";
        return QColor("#FFFFFF");
    }
    if (userId < 0) {
        qDebug() << "getUserColor: userId =" << userId << ", returning #A0A0A0 (invalid)";
        return QColor("#A0A0A0");
    }
    QString color = colorList[userId % colorList.size()];
    qDebug() << "getUserColor: userId =" << userId << ", returning" << color;
    return QColor(color);
}

QString ChatWindow::getUserName(int userId) const {
    if (userId == client->getClientId())
        return "Я";
    if (userId < 0)
        return "User?";
    return QString("User%1").arg(userId);
}

void ChatWindow::setupConnections() {
    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(messageInput, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);
    connect(emojiButton, &QPushButton::clicked, this, &ChatWindow::selectEmoji);
    connect(fileButton, &QPushButton::clicked, this, &ChatWindow::sendFile);
    connect(chatArea, &QTextBrowser::anchorClicked, this, &ChatWindow::handleAnchorClicked);

    connect(leaveButton, &QPushButton::clicked, this, &ChatWindow::onLeaveRoomClicked);
    connect(client, &Client::userJoined, this, &ChatWindow::handleUserJoined);
    connect(client, &Client::userLeft, this, &ChatWindow::handleUserLeft);
    connect(client, &Client::leftRoom, this, &ChatWindow::handleLeftRoom);

    if (client) {
        connect(client, &Client::messageReceived, this, [this](int roomId, int senderId, const QString& message) {
            if (this->roomId == roomId) {
                lastSenderId = senderId;

                QString senderName = getUserName(senderId);
                QColor color = getUserColor(senderId);
                qDebug() << "messageReceived: roomId =" << roomId << ", senderId =" << senderId << ", color =" << color.name() << ", message =" << message;

                QString timeStr = QDateTime::currentDateTime().toString("hh:mm");
                QTextCursor cursor = chatArea->textCursor();
                cursor.movePosition(QTextCursor::End);

                if (lastWasEvent) {
                    chatArea->append("");
                    qDebug() << "messageReceived: Added empty line due to lastWasEvent";
                }

                // Set block alignment to left
                QTextBlockFormat blockFormat;
                blockFormat.setAlignment(Qt::AlignLeft);
                blockFormat.setBottomMargin(15); // Add margin between messages
                cursor.insertBlock(blockFormat);

                // Set color and font for sender name
                QTextCharFormat format;
                format.setForeground(color);
                format.setFont(QFont("Arial", 16, QFont::Bold));
                cursor.setCharFormat(format);
                cursor.insertText(senderName + ": ");

                // Message text
                format.setFont(QFont("Arial", 16));
                cursor.setCharFormat(format);
                cursor.insertText(message + " ");

                // Time in grey
                QTextCharFormat timeFormat;
                timeFormat.setForeground(QColor("#A0A0A0"));
                timeFormat.setFont(QFont("Arial", 10));
                cursor.setCharFormat(timeFormat);
                cursor.insertText(timeStr);

                chatArea->setTextCursor(cursor);
                chatArea->verticalScrollBar()->setValue(chatArea->verticalScrollBar()->maximum());
                lastWasEvent = false;
            }
        });
        connect(client, &Client::errorReceived, this, [this](const QString& error) {
            QMessageBox::warning(this, "Ошибка", error);
        });
        connect(client, &Client::fileReceived, this, [this](int roomId, int senderId, const QString& fileName, const QByteArray& fileData) {
            if (this->roomId == roomId) {
                QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
                QString fileId = QString("%1_%2_%3").arg(fileName).arg(senderId).arg(timestamp);
                FileData fileDataStruct;
                fileDataStruct.fileName = fileName;
                fileDataStruct.data = fileData;
                fileDataMap[fileId] = fileDataStruct;
                lastSenderId = senderId;
                QString senderName = getUserName(senderId);
                QColor color = getUserColor(senderId);
                qDebug() << "fileReceived: roomId =" << roomId << ", senderId =" << senderId << ", color =" << color.name() << ", fileName =" << fileName;

                QString timeStr = QDateTime::currentDateTime().toString("hh:mm");
                QTextCursor cursor = chatArea->textCursor();
                cursor.movePosition(QTextCursor::End);

                if (lastWasEvent) {
                    chatArea->append("");
                    qDebug() << "fileReceived: Added empty line due to lastWasEvent";
                }

                // Set block alignment to left
                QTextBlockFormat blockFormat;
                blockFormat.setAlignment(Qt::AlignLeft);
                blockFormat.setBottomMargin(15); // Add margin between messages
                cursor.insertBlock(blockFormat);

                // Set color and font for sender name
                QTextCharFormat format;
                format.setForeground(color);
                format.setFont(QFont("Arial", 16, QFont::Bold));
                cursor.setCharFormat(format);
                cursor.insertText(senderName + " отправил файл: ");

                // File link
                QString link = QString("<a href=\"file://%1\" style=\"color: #64B5F6; text-decoration: underline; font-weight: bold;\">%2</a>")
                                   .arg(fileId)
                                   .arg(fileName);
                cursor.insertHtml(link + " ");

                // Time in grey
                QTextCharFormat timeFormat;
                timeFormat.setForeground(QColor("#A0A0A0"));
                timeFormat.setFont(QFont("Arial", 10));
                cursor.setCharFormat(timeFormat);
                cursor.insertText(timeStr);

                chatArea->setTextCursor(cursor);
                chatArea->verticalScrollBar()->setValue(chatArea->verticalScrollBar()->maximum());
                lastWasEvent = false;
            }
        });
        connect(client, &Client::fileSent, this, &ChatWindow::handleFileSent);
        connect(client, &Client::fileSentConfirmed, this, []() {});
        connect(client, &Client::usersReceived, this, [this](const QStringList& users) {
            lastUserList = users;
            updateUserList(users);
        });
    }
}

void ChatWindow::updateUserList(const QStringList& users) {
    userList->clear();
    int count = users.size();
    userListLabel->setText(QString("Список участников (%1)").arg(count));
    for (const QString& userStr : users) {
        int userId = -1;
        QString name = userStr;
        if (userStr == "User" + QString::number(client->getClientId())) {
            userId = client->getClientId();
            name = "Я";
        } else {
            QRegularExpression re("User(\\d+)");
            auto match = re.match(userStr);
            if (match.hasMatch()) {
                userId = match.captured(1).toInt();
                if (userId == client->getClientId()) {
                    name = "Я";
                } else {
                    name = QString("User%1").arg(userId);
                }
            }
        }
        QColor dotColor = getUserColor(userId);
        QString dot = QString("<span style='color:%1; font-size:18px; font-weight:bold; vertical-align:middle;'>●</span>").arg(dotColor.name());
        QString userHtml = QString("%1 <span style='font-weight:bold; color:%2;'>%3</span>")
                               .arg(dot)
                               .arg((name == "Я") ? "#FFFFFF" : dotColor.name())
                               .arg(name);
        QListWidgetItem *item = new QListWidgetItem();
        item->setText("");
        item->setData(Qt::DisplayRole, "");
        item->setData(Qt::UserRole, name);
        item->setData(Qt::UserRole + 1, dotColor);
        item->setData(Qt::UserRole + 2, userHtml);
        userList->addItem(item);
        userList->setItemWidget(item, new QLabel(userHtml));
    }
}

void ChatWindow::sendMessage() {
    QString message = messageInput->text().trimmed();
    if (message.isEmpty() || roomId == -1 || !client) return;
    client->sendRequest("MESSAGE:" + std::to_string(roomId) + ":" + message.toStdString());
    messageInput->clear();
}

void ChatWindow::selectEmoji() {
    EmojiPanel *emojiPanel = new EmojiPanel(this);
    connect(emojiPanel, &EmojiPanel::emojiSelected, this, [this](const QString& emoji) {
        messageInput->insert(emoji);
    });
    emojiPanel->show();
}

void ChatWindow::sendFile() {
    QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл");
    if (!filePath.isEmpty() && client) client->sendFile(filePath.toStdString());
}

void ChatWindow::handleFileSent(const QString& fileName, const QByteArray& fileData) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString fileId = QString("%1_%2_%3").arg(fileName).arg(client->getClientId()).arg(timestamp);
    FileData fileDataStruct;
    fileDataStruct.fileName = fileName;
    fileDataStruct.data = fileData;
    fileDataMap[fileId] = fileDataStruct;
    lastSenderId = client->getClientId();
    QString senderName = "Я";
    QColor color = getUserColor(client->getClientId());
    qDebug() << "handleFileSent: senderId =" << client->getClientId() << ", color =" << color.name() << ", fileName =" << fileName;

    QString timeStr = QDateTime::currentDateTime().toString("hh:mm");
    QTextCursor cursor = chatArea->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (lastWasEvent) {
        chatArea->append("");
        qDebug() << "handleFileSent: Added empty line due to lastWasEvent";
    }

    // Set block alignment to left
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment(Qt::AlignLeft);
    blockFormat.setBottomMargin(15); // Add margin between messages
    cursor.insertBlock(blockFormat);

    // Set color and font for sender name
    QTextCharFormat format;
    format.setForeground(color);
    format.setFont(QFont("Arial", 16, QFont::Bold));
    cursor.setCharFormat(format);
    cursor.insertText(senderName + " отправил файл: ");

    // File link
    QString link = QString("<a href=\"file://%1\" style=\"color: #64B5F6; text-decoration: underline; font-weight: bold;\">%2</a>")
                       .arg(fileId)
                       .arg(fileName);
    cursor.insertHtml(link + " ");

    // Time in grey
    QTextCharFormat timeFormat;
    timeFormat.setForeground(QColor("#A0A0A0"));
    timeFormat.setFont(QFont("Arial", 10));
    cursor.setCharFormat(timeFormat);
    cursor.insertText(timeStr);

    chatArea->setTextCursor(cursor);
    chatArea->verticalScrollBar()->setValue(chatArea->verticalScrollBar()->maximum());
    lastWasEvent = false;
}

void ChatWindow::handleAnchorClicked(const QUrl& url) {
    QString fileId = url.toString().mid(7);
    if (fileDataMap.contains(fileId)) {
        QString fileName = fileDataMap[fileId].fileName;
        QString savePath = QFileDialog::getSaveFileName(this, "Сохранить файл", fileName, "Все файлы (*.*)");
        if (!savePath.isEmpty()) {
            QFile file(savePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(fileDataMap[fileId].data);
                file.close();
                QString timeStr = QDateTime::currentDateTime().toString("hh:mm");
                QTextCursor cursor = chatArea->textCursor();
                cursor.movePosition(QTextCursor::End);

                if (lastWasEvent) {
                    chatArea->append("");
                    qDebug() << "handleAnchorClicked: Added empty line due to lastWasEvent";
                }

                // Set block alignment to left
                QTextBlockFormat blockFormat;
                blockFormat.setAlignment(Qt::AlignLeft);
                blockFormat.setBottomMargin(15); // Add margin between messages
                cursor.insertBlock(blockFormat);

                // Set color and font for message
                QTextCharFormat format;
                format.setForeground(QColor("#FFFFFF"));
                format.setFont(QFont("Arial", 16, QFont::Bold));
                cursor.setCharFormat(format);
                cursor.insertText("я сохранил файл: ");
                cursor.insertText(fileName + " ");

                // Time in grey
                QTextCharFormat timeFormat;
                timeFormat.setForeground(QColor("#A0A0A0"));
                timeFormat.setFont(QFont("Arial", 10));
                cursor.setCharFormat(timeFormat);
                cursor.insertText(timeStr);

                chatArea->setTextCursor(cursor);
                chatArea->verticalScrollBar()->setValue(chatArea->verticalScrollBar()->maximum());
                lastWasEvent = false;
            }
        }
    }
}

void ChatWindow::handleUserJoined(int roomId, int userId) {
    if (this->roomId == roomId) {
        QString message = QString("%1 вошел в чат").arg(getUserName(userId));
        QTextCursor cursor = chatArea->textCursor();
        cursor.movePosition(QTextCursor::End);

        QTextBlockFormat blockFormat;
        blockFormat.setAlignment(Qt::AlignCenter);
        blockFormat.setBottomMargin(2);
        cursor.insertBlock(blockFormat);

        QTextCharFormat format;
        format.setForeground(QColor("#8f8f8f"));
        format.setFont(QFont("Arial", 16));
        cursor.setCharFormat(format);
        cursor.insertText(message);

        chatArea->setTextCursor(cursor);
        chatArea->verticalScrollBar()->setValue(chatArea->verticalScrollBar()->maximum());
        lastWasEvent = true;
        client->sendRequest("GET_USERS:" + std::to_string(roomId));
    }
}

void ChatWindow::handleUserLeft(int roomId, int userId) {
    if (this->roomId == roomId) {
        QString message = QString("%1 вышел из чата").arg(getUserName(userId));
        QTextCursor cursor = chatArea->textCursor();
        cursor.movePosition(QTextCursor::End);

        QTextBlockFormat blockFormat;
        blockFormat.setAlignment(Qt::AlignCenter);
        blockFormat.setBottomMargin(2);
        cursor.insertBlock(blockFormat);

        QTextCharFormat format;
        format.setForeground(QColor("#8f8f8f"));
        format.setFont(QFont("Arial", 16));
        cursor.setCharFormat(format);
        cursor.insertText(message);

        chatArea->setTextCursor(cursor);
        chatArea->verticalScrollBar()->setValue(chatArea->verticalScrollBar()->maximum());
        lastWasEvent = true;
        client->sendRequest("GET_USERS:" + std::to_string(roomId));
    }
}

void ChatWindow::onLeaveRoomClicked() {
    if (client && roomId != -1) {
        client->leaveRoom();
    }
    QApplication::quit();
}

void ChatWindow::handleLeftRoom() {
    this->hide();
    MainWindow *mainWindow = new MainWindow();
    mainWindow->show();
    QTimer::singleShot(100, this, [this]() {
        client->disconnect();
        this->deleteLater();
    });
}
