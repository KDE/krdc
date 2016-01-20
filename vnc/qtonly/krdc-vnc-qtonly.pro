TEMPLATE = app
TARGET = krdc-vnc-qtonly
DEPENDPATH += .
INCLUDEPATH += .
LIBS += -lvncclient -lgnutls
DEFINES += QTONLY

QT += core gui widgets

HEADERS += remoteview.h vncclientthread.h vncview.h krdc_debug.h
SOURCES += main.cpp remoteview.cpp vncclientthread.cpp vncview.cpp krdc_debug.cpp
