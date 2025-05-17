#include "joinroomdialog.h"
#include "client.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

JoinRoomDialog::JoinRoomDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Присоединиться к комнате");
    QVBoxLayout *layout = new QVBoxLayout(this);

    ipEdit = new QLineEdit(this);
    ipEdit->setPlaceholderText("IP сервера");
    ipEdit->setText("127.0.0.1");

    portEdit = new QLineEdit(this);
    portEdit->setPlaceholderText("Порт");
    portEdit->setText("12346");

    roomIdEdit = new QLineEdit(this);
    roomIdEdit->setPlaceholderText("ID комнаты");

    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("Пароль (опционально)");
    passwordEdit->setEchoMode(QLineEdit::Password);

    joinButton = new QPushButton("Присоединиться", this);

    layout->addWidget(ipEdit);
    layout->addWidget(portEdit);
    layout->addWidget(roomIdEdit);
    layout->addWidget(passwordEdit);
    layout->addWidget(joinButton);

    connect(joinButton, &QPushButton::clicked, this, &JoinRoomDialog::onJoinClicked);

    // Применяем стиль и размер
    setFixedSize(300, 500);
    setWindowIcon(QIcon(":/icons/snake.png"));
    setStyleSheet(R"(
        QWidget {
            background-color: #12171c;
            color: #ECF0F1;
        }
        QLineEdit {
            background-color: #34495E;
            border: 1px solid #2ECC71;
            border-radius: 5px;
            padding: 5px;
            font-size: 14px;
            font: bold;
            color: #ECF0F1;
        }
        QPushButton {
            background-color: #2ECC71;
            color: #2C3E50;
            border: 1px solid #ECF0F1;
            border-radius: 5px;
            padding: 8px 15px;
            font-weight: bold;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #27AE60;
            border: 1px solid #ECF0F1;
        }
    )");

    qDebug() << "JoinRoomDialog created";
}

void JoinRoomDialog::onJoinClicked() {
    QString ip = ipEdit->text().trimmed();
    QString portStr = portEdit->text().trimmed();
    QString roomIdStr = roomIdEdit->text().trimmed();
    QString password = passwordEdit->text();

    if (ip.isEmpty() || portStr.isEmpty() || roomIdStr.isEmpty()) {
        qDebug() << "Missing input: IP, port, or room ID";
        QMessageBox::warning(this, "Ошибка", "Введите IP, порт и ID комнаты");
        return;
    }

    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        qDebug() << "Invalid port:" << portStr;
        QMessageBox::warning(this, "Ошибка", "Некорректный порт");
        return;
    }

    int roomId = roomIdStr.toInt(&ok);
    if (!ok || roomId <= 0) {
        qDebug() << "Invalid room ID:" << roomIdStr;
        QMessageBox::warning(this, "Ошибка", "Некорректный ID комнаты");
        return;
    }

    Client& client = Client::getInstance();
    qDebug() << "Attempting to connect to" << ip << ":" << port;
    bool isConnected = client.connectToServer(ip.toStdString(), port);
    if (!isConnected) {
        qDebug() << "Connection failed";
        QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к серверу");
        return;
    }

    qDebug() << "Joining room:" << roomId << "with password:" << password;

    bool responseReceived = false;
    connect(&client, &Client::joinedRoom, this, [&responseReceived, this, roomId]() {
        responseReceived = true;
        qDebug() << "Room join successful, room ID:" << roomId;
        accept();
    });
    connect(&client, &Client::errorReceived, this, [&responseReceived, this](const QString& error) {
        responseReceived = true;
        qDebug() << "Error joining room:" << error;
        QMessageBox::warning(this, "Ошибка", error);
        reject();
    });

    client.sendRequest("JOIN_ROOM:" + roomIdStr.toStdString() + ":" + password.toStdString());

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, [&responseReceived, this]() {
        if (!responseReceived) {
            qDebug() << "No response from server for JOIN_ROOM";
            QMessageBox::warning(this, "Ошибка", "Сервер не отвечает");
            reject();
        }
    });
    timer.start(10000);
}

void JoinRoomDialog::setRoomId(const QString& id) {
    roomIdEdit->setText(id);
    roomIdEdit->setEnabled(false); // Запрещаем редактирование, так как комната уже выбрана
}

void JoinRoomDialog::showPasswordField() {
    passwordEdit->setVisible(true);
}

void JoinRoomDialog::hidePasswordField() {
    passwordEdit->setVisible(false);
}
