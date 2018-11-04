#-------------------------------------------------
#
# Project created by QtCreator 2018-11-03T11:17:54
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = duplicate_folder_finder
TEMPLATE = app

CONFIG += c++11
QMAKE_LFLAGS += -static

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h \
    crc.h

FORMS    += mainwindow.ui
