#include "mainwindow.h"
#include "database.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Инициализация интерфейса
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    output = new QTextEdit(this);
    output->setReadOnly(true);

    input = new QLineEdit(this);
    input->setPlaceholderText("Введите SQL-запрос...");

    btnExecute = new QPushButton("Выполнить", this);
    btnMessages = new QPushButton("Показать сообщения", this);
    btnRooms = new QPushButton("Показать комнаты", this);
    btnClear = new QPushButton("Очистить историю", this);  // Новая кнопка

    layout->addWidget(output);
    layout->addWidget(input);
    layout->addWidget(btnExecute);
    layout->addWidget(btnMessages);
    layout->addWidget(btnRooms);
    layout->addWidget(btnClear);  // Добавляем кнопку в layout

    setCentralWidget(centralWidget);
    resize(600, 400);

    // Соединение сигналов
    connect(btnExecute, &QPushButton::clicked, this, &MainWindow::executeQuery);
    connect(input, &QLineEdit::returnPressed, this, &MainWindow::executeQuery);
    connect(btnMessages, &QPushButton::clicked, this, &MainWindow::showMessages);
    connect(btnRooms, &QPushButton::clicked, this, &MainWindow::showRooms);
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearHistory);  // Новый коннект

    // Вывод информации о базе
    output->append("База данных инициализирована");
}

void MainWindow::executeQuery() {
    QString sql = input->text().trimmed();
    if (sql.isEmpty()) return;

    QSqlQuery query(Database::instance().getDb());
    if (query.exec(sql)) {
        output->append("\n> " + sql);

        if (query.isSelect()) {
            // Вывод результатов SELECT
            QString result;
            while (query.next()) {
                for (int i = 0; i < query.record().count(); i++) {
                    result += query.value(i).toString() + "\t";
                }
                result += "\n";
            }
            output->append(result.isEmpty() ? "Нет данных" : result);
        } else {
            output->append("Выполнено успешно. Затронуто строк: " +
                           QString::number(query.numRowsAffected()));
        }
    } else {
        output->append("\nОШИБКА: " + query.lastError().text());
    }

    input->clear();
}

void MainWindow::showMessages() {
    QSqlQuery query(Database::instance().getDb());
    if (query.exec("SELECT * FROM messages")) {
        output->append("\n> SELECT * FROM messages");
        QString result;
        while (query.next()) {
            for (int i = 0; i < query.record().count(); i++) {
                result += query.value(i).toString() + "\t";
            }
            result += "\n";
        }
        output->append(result.isEmpty() ? "Нет данных" : result);
    } else {
        output->append("\nОШИБКА: " + query.lastError().text());
    }
}

void MainWindow::showRooms() {
    QSqlQuery query(Database::instance().getDb());
    if (query.exec("SELECT * FROM rooms")) {
        output->append("\n> SELECT * FROM rooms");
        QString result;
        while (query.next()) {
            for (int i = 0; i < query.record().count(); i++) {
                result += query.value(i).toString() + "\t";
            }
            result += "\n";
        }
        output->append(result.isEmpty() ? "Нет данных" : result);
    } else {
        output->append("\nОШИБКА: " + query.lastError().text());
    }
}

void MainWindow::clearHistory() {
    output->clear();  // Очищаем текстовое поле
    output->append("База данных инициализирована");  // Восстанавливаем начальное сообщение
}
