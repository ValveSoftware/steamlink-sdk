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
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativeconnections_p.h>
#include <private/qdeclarativeitem_p.h>
#include <QtDeclarative/qdeclarativescriptstring.h>

class tst_qdeclarativeconnection : public QObject

{
    Q_OBJECT
public:
    tst_qdeclarativeconnection();

private slots:
    void defaultValues();
    void properties();
    void connection();
    void trimming();
    void targetChanged();
    void unknownSignals_data();
    void unknownSignals();
    void errors_data();
    void errors();

private:
    QDeclarativeEngine engine;
};

tst_qdeclarativeconnection::tst_qdeclarativeconnection()
{
}

void tst_qdeclarativeconnection::defaultValues()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/test-connection3.qml"));
    QDeclarativeConnections *item = qobject_cast<QDeclarativeConnections*>(c.create());

    QVERIFY(item != 0);
    QVERIFY(item->target() == 0);

    delete item;
}

void tst_qdeclarativeconnection::properties()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/test-connection2.qml"));
    QDeclarativeConnections *item = qobject_cast<QDeclarativeConnections*>(c.create());

    QVERIFY(item != 0);

    QVERIFY(item != 0);
    QVERIFY(item->target() == item);

    delete item;
}

void tst_qdeclarativeconnection::connection()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/test-connection.qml"));
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(c.create());

    QVERIFY(item != 0);

    QCOMPARE(item->property("tested").toBool(), false);
    QCOMPARE(item->width(), 50.);
    emit item->setWidth(100.);
    QCOMPARE(item->width(), 100.);
    QCOMPARE(item->property("tested").toBool(), true);

    delete item;
}

void tst_qdeclarativeconnection::trimming()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/trimming.qml"));
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(c.create());

    QVERIFY(item != 0);

    QCOMPARE(item->property("tested").toString(), QString(""));
    int index = item->metaObject()->indexOfSignal("testMe(int,QString)");
    QMetaMethod method = item->metaObject()->method(index);
    method.invoke(item,
                  Qt::DirectConnection,
                  Q_ARG(int, 5),
                  Q_ARG(QString, "worked"));
    QCOMPARE(item->property("tested").toString(), QString("worked5"));

    delete item;
}

// Confirm that target can be changed by one of our signal handlers
void tst_qdeclarativeconnection::targetChanged()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/connection-targetchange.qml"));
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(c.create());
    QVERIFY(item != 0);

    QDeclarativeConnections *connections = item->findChild<QDeclarativeConnections*>("connections");
    QVERIFY(connections);

    QDeclarativeItem *item1 = item->findChild<QDeclarativeItem*>("item1");
    QVERIFY(item1);

    item1->setWidth(200);

    QDeclarativeItem *item2 = item->findChild<QDeclarativeItem*>("item2");
    QVERIFY(item2);
    QVERIFY(connections->target() == item2);

    // If we don't crash then we're OK

    delete item;
}

void tst_qdeclarativeconnection::unknownSignals_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("error");

    QTest::newRow("basic") << "connection-unknownsignals.qml" << ":6:5: QML Connections: Cannot assign to non-existent property \"onFooBar\"";
    QTest::newRow("parent") << "connection-unknownsignals-parent.qml" << ":6:5: QML Connections: Cannot assign to non-existent property \"onFooBar\"";
    QTest::newRow("ignored") << "connection-unknownsignals-ignored.qml" << ""; // should be NO error
    QTest::newRow("notarget") << "connection-unknownsignals-notarget.qml" << ""; // should be NO error
}

void tst_qdeclarativeconnection::unknownSignals()
{
    QFETCH(QString, file);
    QFETCH(QString, error);

    QUrl url = QUrl::fromLocalFile(SRCDIR "/data/" + file);
    if (!error.isEmpty()) {
        QTest::ignoreMessage(QtWarningMsg, (url.toString() + error).toLatin1());
    } else {
        // QTest has no way to insist no message (i.e. fail)
    }

    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, url);
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(c.create());
    QVERIFY(item != 0);

    // check that connection is created (they are all runtime errors)
    QDeclarativeConnections *connections = item->findChild<QDeclarativeConnections*>("connections");
    QVERIFY(connections);

    if (file == "connection-unknownsignals-ignored.qml")
        QVERIFY(connections->ignoreUnknownSignals());

    delete item;
}

void tst_qdeclarativeconnection::errors_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("error");

    QTest::newRow("no \"on\"") << "error-property.qml" << "Cannot assign to non-existent property \"fakeProperty\"";
    QTest::newRow("3rd letter lowercase") << "error-property2.qml" << "Cannot assign to non-existent property \"onfakeProperty\"";
    QTest::newRow("child object") << "error-object.qml" << "Connections: nested objects not allowed";
    QTest::newRow("grouped object") << "error-syntax.qml" << "Connections: syntax error";
}

void tst_qdeclarativeconnection::errors()
{
    QFETCH(QString, file);
    QFETCH(QString, error);

    QUrl url = QUrl::fromLocalFile(SRCDIR "/data/" + file);

    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, url);
    QVERIFY(c.isError() == true);
    QList<QDeclarativeError> errors = c.errors();
    QVERIFY(errors.count() == 1);
    QCOMPARE(errors.at(0).description(), error);
}

QTEST_MAIN(tst_qdeclarativeconnection)

#include "tst_qdeclarativeconnection.moc"
