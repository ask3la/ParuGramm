#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void executeQuery();
    void showMessages();
    void showRooms();
    void clearHistory();  // Новая слот-функция для очистки истории

private:
    QTextEdit *output;
    QLineEdit *input;
    QPushButton *btnExecute;
    QPushButton *btnMessages;
    QPushButton *btnRooms;
    QPushButton *btnClear;  // Новая кнопка для очистки
};

#endif // MAINWINDOW_H
