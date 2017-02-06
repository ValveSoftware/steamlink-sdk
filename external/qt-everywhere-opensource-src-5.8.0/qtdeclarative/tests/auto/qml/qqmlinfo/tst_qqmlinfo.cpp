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
#include <QTimer>
#include <QQmlContext>
#include <qqmlinfo.h>
#include "../../shared/util.h"

class tst_qqmlinfo : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlinfo() {}

private slots:
    void qmlObject();
    void nestedQmlObject();
    void nestedComponent();
    void nonQmlObject();
    void nullObject();
    void nonQmlContextedObject();
    void types();
    void chaining();

private:
    QQmlEngine engine;
};

void tst_qqmlinfo::qmlObject()
{
    QQmlComponent component(&engine, testFileUrl("qmlObject.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QString message = component.url().toString() + ":3:1: QML QtObject: Test Message";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(object) << "Test Message";

    QObject *nested = qvariant_cast<QObject *>(object->property("nested"));
    QVERIFY(nested != 0);

    message = component.url().toString() + ":6:13: QML QtObject: Second Test Message";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(nested) << "Second Test Message";
}

void tst_qqmlinfo::nestedQmlObject()
{
    QQmlComponent component(&engine, testFileUrl("nestedQmlObject.qml"));

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

void tst_qqmlinfo::nestedComponent()
{
    QQmlComponent component(&engine, testFileUrl("NestedComponent.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QObject *nested = qvariant_cast<QObject *>(object->property("nested"));
    QVERIFY(nested != 0);
    QObject *nested2 = qvariant_cast<QObject *>(object->property("nested2"));
    QVERIFY(nested2 != 0);

    QString message = component.url().toString() + ":10:9: QML NestedObject: Complex Object";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(nested) << "Complex Object";

    message = component.url().toString() + ":16:9: QML Image: Simple Object";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    qmlInfo(nested2) << "Simple Object";
}

void tst_qqmlinfo::nonQmlObject()
{
    QObject object;
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML QtObject: Test Message");
    qmlInfo(&object) << "Test Message";

    QTimer nonQmlObject;
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML QTimer: Test Message");
    qmlInfo(&nonQmlObject) << "Test Message";
}

void tst_qqmlinfo::nullObject()
{
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Null Object Test Message");
    qmlInfo(0) << "Null Object Test Message";
}

void tst_qqmlinfo::nonQmlContextedObject()
{
    QObject object;
    QQmlContext context(&engine);
    QQmlEngine::setContextForObject(&object, &context);
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML QtObject: Test Message");
    qmlInfo(&object) << "Test Message";
}

void tst_qqmlinfo::types()
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

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: true");
    qmlInfo(0) << bool(true);

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

void tst_qqmlinfo::chaining()
{
    QString str("Hello World");
    QStringRef ref(&str, 6, 5);
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: false 1.1 1.2 15 hello 'b' World \"Qt\" true Quick QUrl(\"http://www.qt-project.org\") ");
    qmlInfo(0) << false << ' '
               << 1.1 << ' '
               << 1.2f << ' '
               << 15 << ' '
               << QLatin1String("hello") << ' '
               << QChar('b') << ' '
               << ref << ' '
               << QByteArray("Qt") << ' '
               << bool(true) << ' '
               << QString ("Quick") << ' '
               << QUrl("http://www.qt-project.org");
}

QTEST_MAIN(tst_qqmlinfo)

#include "tst_qqmlinfo.moc"
