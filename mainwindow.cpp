#include "mainwindow.h"
#include "createroomdialog.h"
#include "joinroomdialog.h"
#include "chatwindow.h"
#include "client.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QDebug>

#include <QLabel>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("ParuGramm");
    setWindowIcon(QIcon(":/icons/snake.png"));

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Логотип
    QLabel *logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/icons/snake.svg");
    logoLabel->setPixmap(logoPixmap.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);

    // Надпись ParuGramm
    QLabel *titleLabel = new QLabel("ParuGramm", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 70px; font-weight: bold; color: #0BDA51;");

    //кнопочки
    createRoomButton = new QPushButton("Создать комнату", this);
    joinRoomButton = new QPushButton("Присоединиться", this);

    layout->addSpacing(30);
    layout->addWidget(logoLabel);
    layout->addWidget(titleLabel);
    layout->addSpacing(90); // Отступ
    layout->addWidget(createRoomButton);
    layout->addSpacing(20); // Отступ перед кнопками
    layout->addWidget(joinRoomButton);
    layout->addStretch();


    connect(createRoomButton, &QPushButton::clicked, this, &MainWindow::onCreateRoomClicked);
    connect(joinRoomButton, &QPushButton::clicked, this, &MainWindow::onJoinRoomClicked);

    Client& client = Client::getInstance();
    connect(&client, &Client::roomCreated, this, &MainWindow::onRoomCreated);
    connect(&client, &Client::joinedRoom, this, &MainWindow::onJoinedRoom);

    setFixedSize(600, 700);
    setStyleSheet(R"(
        QWidget {
            background-color: #2C3E50;
           _training
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

    qDebug() << "MainWindow created:" << this;
}

MainWindow::~MainWindow() {
    qDebug() << "MainWindow destroyed";
}

void MainWindow::onCreateRoomClicked() {
    qDebug() << "Create room button clicked";
    CreateRoomDialog *dialog = new CreateRoomDialog(this);
    dialog->exec();
    delete dialog;
}

void MainWindow::onJoinRoomClicked() {
    qDebug() << "Join room button clicked";
    JoinRoomDialog *dialog = new JoinRoomDialog(this);
    dialog->exec();
    delete dialog;
}

void MainWindow::onRoomCreated(int roomId) {
    qDebug() << "Room created with ID:" << roomId;
    currentRoomId = roomId;
    // Присоединение теперь в createroomdialog.cpp
}

void MainWindow::onJoinedRoom() {
    Client& client = Client::getInstance();
    int roomId = client.getRoomId();
    qDebug() << "Joined room with ID:" << roomId;
    currentRoomId = roomId;
    ChatWindow *chatWindow = new ChatWindow(roomId, &client, nullptr);
    chatWindow->setAttribute(Qt::WA_DeleteOnClose);
    qDebug() << "ChatWindow created for room ID:" << roomId;
    this->hide();
}
