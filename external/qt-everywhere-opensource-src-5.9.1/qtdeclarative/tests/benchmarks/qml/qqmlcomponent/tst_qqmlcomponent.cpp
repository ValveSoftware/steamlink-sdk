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
#include <QFile>
#include <QDebug>
#include "testtypes.h"

class tst_qmlcomponent : public QObject
{
    Q_OBJECT

public:
    tst_qmlcomponent();
    virtual ~tst_qmlcomponent();

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void creation_data();
    void creation();

private:
    QQmlEngine engine;
};

tst_qmlcomponent::tst_qmlcomponent()
{
}

tst_qmlcomponent::~tst_qmlcomponent()
{
}

void tst_qmlcomponent::initTestCase()
{
    registerTypes();
}

void tst_qmlcomponent::cleanupTestCase()
{
}

void tst_qmlcomponent::creation_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("Object") << SRCDIR "/data/object.qml";
    QTest::newRow("Object - Id") << SRCDIR "/data/object_id.qml";
    QTest::newRow("MyQmlObject") << SRCDIR "/data/myqmlobject.qml";
    QTest::newRow("MyQmlObject: basic binding") << SRCDIR "/data/myqmlobject_binding.qml";
    QTest::newRow("Synthesized properties") << SRCDIR "/data/synthesized_properties.qml";
    QTest::newRow("Synthesized properties.2") << SRCDIR "/data/synthesized_properties.2.qml";
    QTest::newRow("SameGame - BoomBlock") << SRCDIR "/data/samegame/BoomBlock.qml";
}

void tst_qmlcomponent::creation()
{
    QFETCH(QString, file);

    QQmlComponent c(&engine, file);
    QVERIFY(c.isReady());

    QObject *obj = c.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = c.create();
        delete obj;
    }
}

QTEST_MAIN(tst_qmlcomponent)
#include "tst_qqmlcomponent.moc"
