
TARGET = autotests
DESTDIR = ./
TEMPLATE = app

CONFIG -= app_bundle

INCLUDEPATH += shared
DEPENDPATH += shared

SOURCES += autotests.cpp

QT += testlib

OBJECTS_DIR = ../../tmp/obj
UI_DIR = ../../tmp/ui
MOC_DIR = ../../tmp/moc
RCC_DIR = ../../tmp/rcc

include(shared/shared.pri)
include(unittests/unittests.pri)
include(../../sourcelibsdependencies.pri)
