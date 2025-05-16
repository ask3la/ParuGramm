QT       += core gui svg network widgets
CONFIG   += c++17

SOURCES += \
    chatwindow.cpp \
    client.cpp \
    createroomdialog.cpp \
    emojipanel.cpp \
    joinroomdialog.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    chatwindow.h \
    client.h \
    createroomdialog.h \
    emojipanel.h \
    joinroomdialog.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

LIBS += -lws2_32

# Deployment rules
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
