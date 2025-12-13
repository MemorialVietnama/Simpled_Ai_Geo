QT       += core gui concurrent charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Application name
TARGET = SimpledAi

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    fileselectionscene.cpp \
    loaderscene.cpp \
    analysisscene.cpp \
    simplifyscene.cpp \
    comparisonscene.cpp \
    simplification_algorithms.cpp \
    scenemanager.cpp \
    logger.cpp

HEADERS += \
    mainwindow.h \
    fileselectionscene.h \
    loaderscene.h \
    analysisscene.h \
    simplifyscene.h \
    comparisonscene.h \
    simplification_algorithms.h \
    scenemanager.h \
    logger.h


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    tst_general_test.qml
