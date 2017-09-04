/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>
#include <QObject>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>

// Loading QML files from embedded resources is a common QML usecase.
// This test just verifies that it works at a basic level, and with the qrc:/ syntax

class tst_qrcqml : public QObject
{
    Q_OBJECT
public:
    tst_qrcqml();

private slots:
    void basicLoad_data();
    void basicLoad();
    void qrcImport_data();
    void qrcImport();
};

tst_qrcqml::tst_qrcqml()
{
}

void tst_qrcqml::basicLoad_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("token");

    QTest::newRow("simple")
        << "qrc:/data/main.qml"
        << "foo";

    QTest::newRow("aliased")
        << "qrc:/main.qml"
        << "bar";

    QTest::newRow("prefixed")
        << "qrc:/search/main.qml"
        << "baz";

    /* This is not supported:
    QTest::newRow("without qrc scheme")
        << ":/data/main.qml"
        << "hello";
    */
}

void tst_qrcqml::basicLoad()
{
    QFETCH(QString, url);
    QFETCH(QString, token);

    QQmlEngine e;
    QQmlComponent c(&e, QUrl(url));
    QVERIFY(c.isReady());
    QObject* o = c.create();
    QVERIFY(o);
    QCOMPARE(o->property("tokenProperty").toString(), token);
    delete o;
}

void tst_qrcqml::qrcImport_data()
{
    QTest::addColumn<QString>("importPath");
    QTest::addColumn<QString>("token");

    QTest::newRow("qrc path")
        << ":/imports"
        << "foo";

    QTest::newRow("qrc url")
        << "qrc:/imports"
        << "foo";
}

void tst_qrcqml::qrcImport()
{
    QFETCH(QString, importPath);
    QFETCH(QString, token);

    QQmlEngine e;
    e.addImportPath(importPath);
    QQmlComponent c(&e, QUrl("qrc:///importtest.qml"));
    QVERIFY(c.isReady());
    QObject *o = c.create();
    QVERIFY(o);
    QCOMPARE(o->property("tokenProperty").toString(), token);
    delete o;
}

QTEST_MAIN(tst_qrcqml)

#include "tst_qrcqml.moc"
