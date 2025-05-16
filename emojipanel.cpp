#include "emojipanel.h"
#include <QPushButton>
#include <QGridLayout>
#include <QFont>
#include <QDebug>
#include <QApplication>
#include <QScreen>

EmojiPanel::EmojiPanel(QWidget *parent) : QWidget(parent) {
    setupUI();
    setWindowFlags(Qt::Popup);
    setFixedSize(520, 280); // 10x5 кнопок (40x40) + отступы
    setStyleSheet("background: #0c1217; border: 2px solid #0ef16e; border-radius: 5px;");
    setWindowIcon(QIcon(":/icons/snake.png"));

    // Центрирование на экране
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    qDebug() << "EmojiPanel created";
}

void EmojiPanel::setupUI() {
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(5);

    const QStringList emojis = {
        "😀", "😁", "😂", "🤣", "😃", "😄", "😅", "😆", "😉", "😊",
        "😋", "😎", "😍", "😘", "😗", "😙", "😚", "🙂", "🤗", "🤩",
        "🤔", "😐", "😑", "😶", "🙄", "😏", "😣", "😥", "😮", "🤐",
        "😯", "😪", "😫", "😴", "😌", "😛", "😜", "😝", "🤤", "😒",
        "😓", "😔", "😕", "🙃", "🤑", "😲", "😷", "🤒", "🤕", "💀"
    };

    int index = 0;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 10; ++col) {
            if (index >= emojis.size()) break;
            QPushButton *btn = new QPushButton(emojis[index], this);
            btn->setFont(QFont("Segoe UI Emoji", 48));
            btn->setFixedSize(48, 48);
            btn->setStyleSheet("QPushButton:hover { background: #2ECC71; }");
            connect(btn, &QPushButton::clicked, [this, btn]() {
                qDebug() << "Emoji button clicked:" << btn->text();
                emit emojiSelected(btn->text());
            });
            layout->addWidget(btn, row, col);
            index++;
        }
    }
}
