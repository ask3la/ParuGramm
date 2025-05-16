#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include "client.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCreateRoomClicked();
    void onJoinRoomClicked();
    void onRoomCreated(int roomId);
    void onJoinedRoom();

private:
    QPushButton *createRoomButton;
    QPushButton *joinRoomButton;
    Client& client = Client::getInstance();
    int currentRoomId = -1;
};

#endif // MAINWINDOW_H
