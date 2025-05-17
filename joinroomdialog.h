#ifndef JOINROOMDIALOG_H
#define JOINROOMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class JoinRoomDialog : public QDialog {
    Q_OBJECT
public:
    explicit JoinRoomDialog(QWidget *parent = nullptr);

    void setRoomId(const QString& id);
    void showPasswordField();
    void hidePasswordField();

private slots:
    void onJoinClicked();

private:
    QLineEdit *ipEdit;
    QLineEdit *portEdit;
    QLineEdit *roomIdEdit;
    QLineEdit *passwordEdit;
    QPushButton *joinButton;
};

#endif // JOINROOMDIALOG_H
