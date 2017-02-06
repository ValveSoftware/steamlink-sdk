/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>
#include <QtQuick>
#include <QtCore/private/qhooks_p.h>
#include <iostream>

static int qt_verbose = qgetenv("VERBOSE").toInt() != 0;

Q_GLOBAL_STATIC(QObjectList, qt_qobjects)

extern "C" Q_DECL_EXPORT void qt_addQObject(QObject *object)
{
    qt_qobjects->append(object);
}

extern "C" Q_DECL_EXPORT void qt_removeQObject(QObject *object)
{
    qt_qobjects->removeAll(object);
}

class tst_ObjectCount : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void calendar();
    void calendar_data();

    void controls();
    void controls_data();

    void material();
    void material_data();

    void universal();
    void universal_data();

private:
    QQmlEngine engine;
};

void tst_ObjectCount::init()
{
    qtHookData[QHooks::AddQObject] = reinterpret_cast<quintptr>(&qt_addQObject);
    qtHookData[QHooks::RemoveQObject] = reinterpret_cast<quintptr>(&qt_removeQObject);

    // warmup
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; import QtQuick.Controls 2.1; Item { Button {} }", QUrl());
    delete component.create();
}

void tst_ObjectCount::cleanup()
{
    qtHookData[QHooks::AddQObject] = 0;
    qtHookData[QHooks::RemoveQObject] = 0;
}

static void printItems(const QList<QQuickItem *> &items)
{
    std::cout << "RESULT tst_ObjectCount::" << QTest::currentTestFunction() << "():\"" << QTest::currentDataTag() << "\":" << std::endl;
    std::cout << "     QQuickItems: " << items.count() << " (total of QObjects: " << qt_qobjects->count() << ")" << std::endl;

    if (qt_verbose) {
        for (QObject *object : qAsConst(*qt_qobjects))
            qInfo() << "\t" << object;
    }
}

static void addTestRows(QQmlEngine *engine, const QString &sourcePath, const QString &targetPath, const QStringList &skiplist = QStringList())
{
    // We cannot use QQmlComponent to load QML files directly from the source tree.
    // For styles that use internal QML types (eg. material/Ripple.qml), the source
    // dir would be added as an "implicit" import path overriding the actual import
    // path (qtbase/qml/QtQuick/Controls.2/Material). => The QML engine fails to load
    // the style C++ plugin from the implicit import path (the source dir).
    //
    // Therefore we only use the source tree for finding out the set of QML files that
    // a particular style implements, and then we locate the respective QML files in
    // the engine's import path. This way we can use QQmlComponent to load each QML file
    // for benchmarking.

    const QFileInfoList entries = QDir(QQC2_IMPORT_PATH "/" + sourcePath).entryInfoList(QStringList("*.qml"), QDir::Files);
    for (const QFileInfo &entry : entries) {
        QString name = entry.baseName();
        if (!skiplist.contains(name)) {
            const auto importPathList = engine->importPathList();
            for (const QString &importPath : importPathList) {
                QString name = entry.dir().dirName() + "/" + entry.fileName();
                QString filePath = importPath + "/" + targetPath + "/" + entry.fileName();
                if (QFile::exists(filePath)) {
                    QTest::newRow(qPrintable(name)) << QUrl::fromLocalFile(filePath);
                    break;
                } else if (QFile::exists(QQmlFile::urlToLocalFileOrQrc(filePath))) {
                    QTest::newRow(qPrintable(name)) << QUrl(filePath);
                    break;
                }
            }
        }
    }
}

static void doBenchmark(QQmlEngine *engine, const QUrl &url)
{
    QQmlComponent component(engine);

    qt_qobjects->clear();

    component.loadUrl(url);
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object.data(), qPrintable(component.errorString()));

    QList<QQuickItem *> items;
    for (QObject *object : qAsConst(*qt_qobjects)) {
        QQuickItem *item = qobject_cast<QQuickItem *>(object);
        if (item)
            items += item;
    }
    printItems(items);
}

void tst_ObjectCount::calendar()
{
    QFETCH(QUrl, url);
    doBenchmark(&engine, url);
}

void tst_ObjectCount::calendar_data()
{
    QTest::addColumn<QUrl>("url");
    addTestRows(&engine, "calendar", "Qt/labs/calendar");
}

void tst_ObjectCount::controls()
{
    QFETCH(QUrl, url);
    doBenchmark(&engine, url);
}

void tst_ObjectCount::controls_data()
{
    QTest::addColumn<QUrl>("url");
    addTestRows(&engine, "controls", "QtQuick/Controls.2", QStringList() << "CheckIndicator" << "RadioIndicator" << "SwitchIndicator");
}

void tst_ObjectCount::material()
{
    QFETCH(QUrl, url);
    doBenchmark(&engine, url);
}

void tst_ObjectCount::material_data()
{
    QTest::addColumn<QUrl>("url");
    addTestRows(&engine, "controls/material", "QtQuick/Controls.2/Material", QStringList() << "Ripple" << "SliderHandle" << "CheckIndicator" << "RadioIndicator" << "SwitchIndicator" << "BoxShadow" << "ElevationEffect");
}

void tst_ObjectCount::universal()
{
    QFETCH(QUrl, url);
    doBenchmark(&engine, url);
}

void tst_ObjectCount::universal_data()
{
    QTest::addColumn<QUrl>("url");
    addTestRows(&engine, "controls/universal", "QtQuick/Controls.2/Universal", QStringList() << "CheckIndicator" << "RadioIndicator" << "SwitchIndicator");
}

QTEST_MAIN(tst_ObjectCount)

#include "tst_objectcount.moc"
