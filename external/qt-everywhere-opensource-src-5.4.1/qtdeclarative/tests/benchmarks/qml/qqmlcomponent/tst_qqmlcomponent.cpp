/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
