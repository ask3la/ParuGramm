#ifndef EMOJIPANEL_H
#define EMOJIPANEL_H

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>

class EmojiPanel : public QWidget {
    Q_OBJECT

public:
    explicit EmojiPanel(QWidget *parent = nullptr);

signals:
    void emojiSelected(const QString &emoji);

private:
    void setupUI();
};

#endif // EMOJIPANEL_H
