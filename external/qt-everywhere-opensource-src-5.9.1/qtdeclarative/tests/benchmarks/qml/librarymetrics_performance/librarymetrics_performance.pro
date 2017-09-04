CONFIG += benchmark
TEMPLATE = app
TARGET = tst_librarymetrics_performance

QT += qml quick network testlib
macx:CONFIG -= app_bundle

CONFIG += release
SOURCES += tst_librarymetrics_performance.cpp

RESOURCES += data
