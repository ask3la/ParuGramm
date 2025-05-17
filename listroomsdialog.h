#ifndef LISTROOMSDIALOG_H
#define LISTROOMSDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include "client.h"

class ListRoomsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ListRoomsDialog(QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onJoinClicked();
    void onRoomListReceived(const QStringList& rooms);

private:
    QTableWidget *roomsTable;
    QPushButton *refreshButton;
    QPushButton *joinButton;
    Client& client;
};

#endif // LISTROOMSDIALOG_H
