# -------------------------------------------------
# Project created by QtCreator 2016-03-31T16:51:26
# -------------------------------------------------
QT += core gui
QT += printsupport
QT += webkitwidgets
QT += webkit
QT += widgets \
    network
greaterThan(QT_MAJOR_VERSION, 4):QT += widgets
TARGET = Terminal1
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    hled.cpp \
    console.cpp \
    gps.cpp \
    sensor.cpp \
    filedata.cpp \
    tranceiver.cpp \
    Dialog/retasksensordialog.cpp \
    Dialog/retskalldialog.cpp \
    Dialog/sensorinformationdialog.cpp \
    Dialog/newsensordialog.cpp \
    Dialog/editsensorplacedialog.cpp \
    Dialog/startuplocationdialog.cpp \
    Dialog/setupserialportdialog.cpp \
    Dialog/aboutdialog.cpp \
    data.cpp \
    Dialog/serverdialog.cpp \
    receivefromweb.cpp
HEADERS += mainwindow.h \
    hled.h \
    console.h \
    gps.h \
    sensor.h \
    define.h \
    filedata.h \
    tranceiver.h \
    Dialog/retasksensordialog.h \
    Dialog/retskalldialog.h \
    Dialog/sensorinformationdialog.h \
    Dialog/newsensordialog.h \
    Dialog/editsensorplacedialog.h \
    Dialog/startuplocationdialog.h \
    Dialog/setupserialportdialog.h \
    Dialog/aboutdialog.h \
    data.h \
    Dialog/serverdialog.h \
    receivefromweb.h
FORMS += mainwindow.ui \
    Dialog/retasksensordialog.ui \
    Dialog/retskalldialog.ui \
    Dialog/sensorinformationdialog.ui \
    Dialog/newsensordialog.ui \
    Dialog/editsensorplacedialog.ui \
    Dialog/startuplocationdialog.ui \
    Dialog/setupserialportdialog.ui \
    Dialog/aboutdialog.ui \
    Dialog/serverdialog.ui
include(qextserialport/qextserialport.pri)
RESOURCES += resources.qrc
OTHER_FILES += html_js/tools.js \
    html_js/init.js \
    html_js/index.html
