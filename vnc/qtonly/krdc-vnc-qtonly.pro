TEMPLATE = app
TARGET = krdc-vnc-qtonly
DEPENDPATH += .
INCLUDEPATH += .
LIBS += -lvncclient -lgnutls
DEFINES += QTONLY

QT += core gui widgets

HEADERS += remoteview.h vncclientthread.h vncview.h logging.h
SOURCES += main.cpp remoteview.cpp vncclientthread.cpp vncview.cpp logging.cpp
