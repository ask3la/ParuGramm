#include "createroomdialog.h"
#include "client.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

CreateRoomDialog::CreateRoomDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Создать комнату");
    QVBoxLayout *layout = new QVBoxLayout(this);

    roomNameEdit = new QLineEdit(this);
    roomNameEdit->setPlaceholderText("Название комнаты");
    roomNameEdit->setStyleSheet("padding: 12px; font-family: 'Roboto';");

    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("Пароль (опционально)");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet("padding: 12px; font-family: 'Roboto';");

    createButton = new QPushButton("Создать", this);
    createButton->setStyleSheet("padding: 12px; font-family: 'Roboto';");

    layout->addWidget(roomNameEdit);
    layout->addWidget(passwordEdit);
    layout->addWidget(createButton);

    connect(createButton, &QPushButton::clicked, this, &CreateRoomDialog::onCreateClicked);

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
}

void CreateRoomDialog::onCreateClicked() {
    QString name = roomNameEdit->text().trimmed();
    password = passwordEdit->text(); // Сохраняем введённый пароль
    if (name.isEmpty()) {
        qDebug() << "Room name is empty";
        QMessageBox::warning(this, "Ошибка", "Введите название комнаты");
        return;
    }

    Client& client = Client::getInstance();
    bool isConnected = client.connectToServer("127.0.0.1", 12346);
    if (!isConnected) {
        qDebug() << "Failed to connect to server";
        QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к серверу");
        return;
    }

    qDebug() << "Creating room:" << name << "with password:" << password;

    bool responseReceived = false;
    connect(&client, &Client::roomCreated, this, [&responseReceived, this, name]() {
        responseReceived = true;
        int roomId = Client::getInstance().getRoomId();
        qDebug() << "Room creation successful, room ID:" << roomId;
        // Присоединяемся с правильным паролем
        Client::getInstance().sendRequest("JOIN_ROOM:" + std::to_string(roomId) + ":" + this->password.toStdString());
        accept();
    });
    connect(&client, &Client::errorReceived, this, [&responseReceived, this](const QString& error) {
        responseReceived = true;
        qDebug() << "Error creating room:" << error;
        QMessageBox::warning(this, "Ошибка", error);
        reject();
    });

    client.sendRequest("CREATE_ROOM:" + name.toStdString() + ":" + password.toStdString());

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, [&responseReceived, this]() {
        if (!responseReceived) {
            qDebug() << "No response from server for CREATE_ROOM";
            QMessageBox::warning(this, "Ошибка", "Сервер не отвечает");
            reject();
        }
    });
    timer.start(10000);
}

QString CreateRoomDialog::getPassword() const {
    return password;
}
