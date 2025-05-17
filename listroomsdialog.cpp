#include "listroomsdialog.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <joinroomdialog.h>
#include <QTimer>
#include <client.h>
#include <QFont>

ListRoomsDialog::ListRoomsDialog(QWidget *parent)
    : QDialog(parent), client(Client::getInstance()) {
    setWindowTitle("Список комнат");
    setMinimumSize(900, 650); // Увеличили размер окна
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint); // Убираем кнопку помощи

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15); // Отступы по краям
    layout->setSpacing(15); // Расстояние между элементами

    // Настройка таблицы
    roomsTable = new QTableWidget(this);
    roomsTable->setColumnCount(4);
    roomsTable->setHorizontalHeaderLabels({"ID", "Название комнаты", "Пароль", "Участники"});

    // Настройка заголовков
    QFont headerFont = roomsTable->horizontalHeader()->font();
    headerFont.setBold(true);
    headerFont.setPointSize(10);
    roomsTable->horizontalHeader()->setFont(headerFont);

    roomsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // Растягиваем на всю ширину
    roomsTable->horizontalHeader()->setStretchLastSection(true); // Последняя колонка растягивается
    roomsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    roomsTable->setEditTriggers(QAbstractItemView::NoEditTriggers); // Запрещаем редактирование
    roomsTable->setSortingEnabled(true);
    roomsTable->sortByColumn(3, Qt::DescendingOrder);
    roomsTable->verticalHeader()->setVisible(false); // Скрываем вертикальные заголовки
    roomsTable->setSelectionMode(QAbstractItemView::SingleSelection); // Выбор только одной строки

    // Настройка кнопок
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(50); // Расстояние между кнопками

    refreshButton = new QPushButton("Обновить", this);
    refreshButton->setFixedWidth(150); // Фиксированная ширина
    refreshButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    joinButton = new QPushButton("Войти", this);
    joinButton->setFixedWidth(150); // Фиксированная ширина
    joinButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    buttonLayout->addStretch(); // Добавляем растягивающееся пространство
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(joinButton);
    buttonLayout->addStretch(); // Добавляем растягивающееся пространство

    layout->addWidget(roomsTable);
    layout->addLayout(buttonLayout);

    connect(refreshButton, &QPushButton::clicked, this, &ListRoomsDialog::onRefreshClicked);
    connect(joinButton, &QPushButton::clicked, this, &ListRoomsDialog::onJoinClicked);
    connect(&client, &Client::roomListReceived, this, &ListRoomsDialog::onRoomListReceived);

    // Таймер для автообновления
    QTimer *refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &ListRoomsDialog::onRefreshClicked);
    refreshTimer->start(30000);

    // Первоначальная загрузка
    onRefreshClicked();

    // Стилизация
    setStyleSheet(R"(
        QDialog {
            background-color: #1c2935;
        }

        QTableWidget {
            background-color: #34495E;
            border: 2px solid #2C3E50;
            color: #ECF0F1;
            font: bold;
            font-size: 12px;
            selection-background-color: #2980B9;
            selection-color: white;
            gridline-color: #2C3E50;
        }

        QHeaderView::section {
            background-color: #2C3E50;
            color: #ECF0F1;
            padding: 8px;
            border: none;
            font-weight: bold;
        }

        QPushButton {
            border-radius: 4px;
            padding: 8px 15px;
            font-weight: bold;
            font-size: 14px;
            min-width: 80px;
        }

        QPushButton#refreshButton {
            background-color: #3498DB;
            color: white;
        }

        QPushButton#refreshButton:hover {
            background-color: #2980B9;
        }

        QPushButton#refreshButton:pressed {
            background-color: #1B6CA8;
        }

        QPushButton#joinButton {
            background-color: #2ECC71;
            color: #2C3E50;
        }

        QPushButton#joinButton:hover {
            background-color: #27AE60;
        }

        QPushButton#joinButton:pressed {
            background-color: #219653;
        }
    )");

    // Присваиваем имена для стилизации
    refreshButton->setObjectName("refreshButton");
    joinButton->setObjectName("joinButton");
}

// Остальные методы остаются без изменений
void ListRoomsDialog::onRefreshClicked() {
    if (!client.isConnected()) {
        bool connected = client.connectToServer("127.0.0.1", 12346);
        if (!connected) {
            QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к серверу");
            return;
        }
    }
    client.sendRequest("LIST_ROOMS");
}

void ListRoomsDialog::onRoomListReceived(const QStringList& rooms) {
    roomsTable->setRowCount(0);
    roomsTable->setSortingEnabled(false);

    for (const QString& roomStr : rooms) {
        QStringList parts = roomStr.split(':');
        if (parts.size() == 4) {
            int row = roomsTable->rowCount();
            roomsTable->insertRow(row);

            // ID
            QTableWidgetItem *idItem = new QTableWidgetItem(parts[0]);
            idItem->setTextAlignment(Qt::AlignCenter);
            roomsTable->setItem(row, 0, idItem);

            // Название
            QTableWidgetItem *nameItem = new QTableWidgetItem(parts[1]);
            nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            roomsTable->setItem(row, 1, nameItem);

            // Пароль
            QTableWidgetItem *passItem = new QTableWidgetItem(parts[2] == "Yes" ? "🔒" : "🔓");
            passItem->setTextAlignment(Qt::AlignCenter);
            roomsTable->setItem(row, 2, passItem);

            // Участники
            QTableWidgetItem *partItem = new QTableWidgetItem(parts[3]);
            partItem->setTextAlignment(Qt::AlignCenter);
            roomsTable->setItem(row, 3, partItem);
        }
    }

    roomsTable->setSortingEnabled(true);
    roomsTable->sortByColumn(3, Qt::DescendingOrder);
}

void ListRoomsDialog::onJoinClicked() {
    QModelIndexList selected = roomsTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите комнату из списка");
        return;
    }

    int row = selected.first().row();
    QString roomId = roomsTable->item(row, 0)->text();
    bool hasPassword = (roomsTable->item(row, 2)->text() == "🔒");

    JoinRoomDialog joinDialog(this);
    joinDialog.setRoomId(roomId);

    if (hasPassword) {
        joinDialog.showPasswordField();
    } else {
        joinDialog.hidePasswordField();
    }

    if (joinDialog.exec() == QDialog::Accepted) {
        accept();
    }
}
