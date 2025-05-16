#include "databaseserver.h"
#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    DatabaseServer::getInstance().start(12346);
    MainWindow w;
    w.show();
    return a.exec();
}
