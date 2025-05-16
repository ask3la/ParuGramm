#ifndef CREATEROOMDIALOG_H
#define CREATEROOMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class CreateRoomDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateRoomDialog(QWidget *parent = nullptr);
    QString getPassword() const; // Добавляем метод для получения пароля

private slots:
    void onCreateClicked();

private:
    QLineEdit *roomNameEdit;
    QLineEdit *passwordEdit;
    QPushButton *createButton;
    QString password; // Добавляем переменную для пароля
};

#endif // CREATEROOMDIALOG_H
