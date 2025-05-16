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

private:
    QTextEdit *output;
    QLineEdit *input;
    QPushButton *btnExecute;
};

#endif // MAINWINDOW_H
