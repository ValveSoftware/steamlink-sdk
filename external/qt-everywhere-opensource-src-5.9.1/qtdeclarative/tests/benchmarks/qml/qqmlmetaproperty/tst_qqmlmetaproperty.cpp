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
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlProperty>
#include <QFile>
#include <QDebug>

class tst_qmlmetaproperty : public QObject
{
    Q_OBJECT

public:
    tst_qmlmetaproperty();
    virtual ~tst_qmlmetaproperty();

public slots:
    void init();
    void cleanup();

private slots:
    void lookup_data();
    void lookup();

private:
    QQmlEngine engine;
};

tst_qmlmetaproperty::tst_qmlmetaproperty()
{
}

tst_qmlmetaproperty::~tst_qmlmetaproperty()
{
}

void tst_qmlmetaproperty::init()
{
}

void tst_qmlmetaproperty::cleanup()
{
}

void tst_qmlmetaproperty::lookup_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("Simple Object") << SRCDIR "/data/object.qml";
    QTest::newRow("Synthesized Object") << SRCDIR "/data/synthesized_object.qml";
}

void tst_qmlmetaproperty::lookup()
{
    QFETCH(QString, file);

    QQmlComponent c(&engine, file);
    QVERIFY(c.isReady());

    QObject *obj = c.create();

    QBENCHMARK {
        QQmlProperty p(obj, "x");
    }

    delete obj;
}

QTEST_MAIN(tst_qmlmetaproperty)
#include "tst_qqmlmetaproperty.moc"
