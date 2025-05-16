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

ChatWindow::ChatWindow(int roomId, Client *client, QWidget *parent) : QWidget(parent), roomId(roomId), client(client) {
    setWindowTitle("Чат: Комната " + QString::number(roomId));
    setFixedSize(800, 875);
    setWindowIcon(QIcon(":/icons/snake.png"));

    // Центрирование на экране
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *inputLayout = new QHBoxLayout();

    // Заголовок с логотипом
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/icons/snake.png");
    logoLabel->setPixmap(logoPixmap.scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QLabel *titleLabel = new QLabel("ParuGramm Chat", this);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2ECC71;");
    headerLayout->addWidget(logoLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    // Чат
    chatArea = new QTextEdit(this);
    chatArea->setReadOnly(true);
    chatArea->setMinimumHeight(450); // Большой чат
    chatArea->setStyleSheet("font-size: 25px;"); // Увеличенный шрифт

    // Список пользователей
    userList = new QListWidget(this);
    userList->addItems({"User1", "User2", "User3"}); // Заглушка
    userList->setMaximumHeight(115); // Компактный список
    userList->setStyleSheet("QListWidget::item { padding: 5px; } QListWidget::item:selected { background: #2ECC71; color: #2C3E50; }");

    // Поле ввода и кнопки
    messageInput = new QLineEdit(this);
    messageInput->setMinimumHeight(40); // Большее поле ввода
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

    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(chatArea);
    mainLayout->addWidget(userList);
    mainLayout->addLayout(inputLayout);

    setLayout(mainLayout);
    setupConnections();

    // Применяем стиль
    setStyleSheet(R"(
        QWidget {
            background-color: #2C3E50;
            color: #ECF0F1;
        }
        QTextEdit, QLineEdit, QListWidget {
            background-color: #34495E;
            border: 1px solid #2ECC71;
            border-radius: 5px;
            padding: 5px;
            font-size: 16px;
            color: #ECF0F1;
        }
        QPushButton {
            background-color: #2ECC71;
            color: #2C3E50;
            border: 1px solid #ECF0F1;
            border-radius: 5px;
            padding: 8px;
            font-weight: bold;
            font-size: 16px;
            box-shadow: 2px 2px 5px rgba(0, 0, 0, 0.5);
        }
        QPushButton:hover {
            background-color: #27AE60;
            border: 1px solid #ECF0F1;
            box-shadow: 4px 4px 8px rgba(0, 0, 0, 0.7);
        }
        QPushButton:pressed {
            background-color: #219653;
        }
    )");

    qDebug() << "ChatWindow created for room ID:" << roomId;
    show();
}

void ChatWindow::setupConnections() {
    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(messageInput, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);
    connect(emojiButton, &QPushButton::clicked, this, &ChatWindow::selectEmoji);
    connect(fileButton, &QPushButton::clicked, this, &ChatWindow::sendFile);

    if (client) {
        connect(client, &Client::messageReceived, this, [this](int roomId, int senderId, const QString& message) {
            qDebug() << "Message received for room" << roomId << "from sender" << senderId << ":" << message;
            if (this->roomId == roomId) {
                chatArea->append(QString("User%1: %2").arg(senderId).arg(message));
            } else {
                qDebug() << "Message ignored: roomId mismatch (chatRoomId:" << this->roomId << ", messageRoomId:" << roomId << ")";
            }
        });
        connect(client, &Client::errorReceived, this, [this](const QString& error) {
            qDebug() << "Error received in ChatWindow:" << error;
            QMessageBox::warning(this, "Ошибка", error);
        });
        connect(client, &Client::fileReceived, this, [this](const QString& fileName, const QByteArray& fileData) {
            qDebug() << "File received:" << fileName;
            QString savePath = QFileDialog::getSaveFileName(this, "Сохранить файл", fileName);
            if (!savePath.isEmpty()) {
                QFile file(savePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(fileData);
                    file.close();
                    chatArea->append(QString("Получен файл: %1").arg(fileName));
                } else {
                    qDebug() << "Failed to save file:" << savePath;
                    QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл");
                }
            }
        });
    }
}

void ChatWindow::sendMessage() {
    QString message = messageInput->text().trimmed();
    if (message.isEmpty() || roomId == -1 || !client) {
        qDebug() << "Cannot send message: empty message or invalid room ID";
        return;
    }

    qDebug() << "Sending message to room" << roomId << ":" << message;
    client->sendRequest("MESSAGE:" + std::to_string(roomId) + ":" + message.toStdString());
    messageInput->clear();
}

void ChatWindow::selectEmoji() {
    qDebug() << "Opening emoji panel";
    EmojiPanel *emojiPanel = new EmojiPanel(this);
    connect(emojiPanel, &EmojiPanel::emojiSelected, this, [this](const QString& emoji) {
        qDebug() << "Emoji selected:" << emoji;
        messageInput->insert(emoji);
    });
    emojiPanel->show();
}

void ChatWindow::sendFile() {
    QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл");
    if (!filePath.isEmpty() && client) {
        qDebug() << "Sending file:" << filePath;
        client->sendFile(filePath.toStdString());
        chatArea->append(QString("Отправлен файл: %1").arg(QFileInfo(filePath).fileName()));
    }
}
