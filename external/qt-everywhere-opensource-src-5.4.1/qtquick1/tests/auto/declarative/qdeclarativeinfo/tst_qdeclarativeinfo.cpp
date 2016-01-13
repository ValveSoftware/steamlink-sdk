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
#include <qdeclarativedatatest.h>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QPushButton>
#include <QDeclarativeContext>
#include <qdeclarativeinfo.h>

class tst_qdeclarativeinfo : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativeinfo() {}

private slots:
    void qmlObject();
    void nestedQmlObject();
    void nonQmlObject();
    void nullObject();
    void nonQmlContextedObject();
    void types();
    void chaining();

private:
    QDeclarativeEngine engine;
};

void tst_qdeclarativeinfo::qmlObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("qmlObject.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QString message = component.url().toString() + ":3:1: QML QObject_QML_0: Test Message";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(object) << "Test Message";

    QObject *nested = qvariant_cast<QObject *>(object->property("nested"));
    QVERIFY(nested != 0);

    message = component.url().toString() + ":6:13: QML QtObject: Second Test Message";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(nested) << "Second Test Message";
}

void tst_qdeclarativeinfo::nestedQmlObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("nestedQmlObject.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QObject *nested = qvariant_cast<QObject *>(object->property("nested"));
    QVERIFY(nested != 0);
    QObject *nested2 = qvariant_cast<QObject *>(object->property("nested2"));
    QVERIFY(nested2 != 0);

    QString message = component.url().toString() + ":5:13: QML NestedObject: Outer Object";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(nested) << "Outer Object";

    message = testFileUrl("NestedObject.qml").toString() + ":6:14: QML QtObject: Inner Object";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(nested2) << "Inner Object";
}

void tst_qdeclarativeinfo::nonQmlObject()
{
    QObject object;
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML QtObject: Test Message");
    qmlInfo(&object) << "Test Message";

    QPushButton pbObject;
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML QPushButton: Test Message");
    qmlInfo(&pbObject) << "Test Message";
}

void tst_qdeclarativeinfo::nullObject()
{
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Null Object Test Message");
    qmlInfo(0) << "Null Object Test Message";
}

void tst_qdeclarativeinfo::nonQmlContextedObject()
{
    QObject object;
    QDeclarativeContext context(&engine);
    QDeclarativeEngine::setContextForObject(&object, &context);
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML QtObject: Test Message");
    qmlInfo(&object) << "Test Message";
}

void tst_qdeclarativeinfo::types()
{
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: false");
    qmlInfo(0) << false;

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: 1.1");
    qmlInfo(0) << 1.1;

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: 1.2");
    qmlInfo(0) << 1.2f;

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: 15");
    qmlInfo(0) << 15;

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: 'b'");
    qmlInfo(0) << QChar('b');

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: \"Qt\"");
    qmlInfo(0) << QByteArray("Qt");

    //### do we actually want QUrl to show up in the output?
    //### why the extra space at the end?
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QUrl(\"http://www.qt-project.org\") ");
    qmlInfo(0) << QUrl("http://www.qt-project.org");

    //### should this be quoted?
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: hello");
    qmlInfo(0) << QLatin1String("hello");

    //### should this be quoted?
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: World");
    QString str("Hello World");
    QStringRef ref(&str, 6, 5);
    qmlInfo(0) << ref;

    //### should this be quoted?
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Quick");
    qmlInfo(0) << QString ("Quick");
}

void tst_qdeclarativeinfo::chaining()
{
    //### should more of these be automatically inserting spaces?
    QString str("Hello World");
    QStringRef ref(&str, 6, 5);
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: false 1.1 1.2 15 hello 'b' QUrl(\"http://www.qt-project.org\") World \"Qt\" Quick ");
    qmlInfo(0) << false << ' '
               << 1.1 << ' '
               << 1.2f << ' '
               << 15 << ' '
               << QLatin1String("hello") << ' '
               << QChar('b') << ' '
               << QUrl("http://www.qt-project.org")
               << ref
               << QByteArray("Qt")
               << QString ("Quick");
}

QTEST_MAIN(tst_qdeclarativeinfo)

#include "tst_qdeclarativeinfo.moc"
