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

#include <qqmlengine.h>
#include <qquickitem.h>
#include <qquickview.h>

#include "../../shared/util.h"
#include "mypropertymap.h"

class tst_SignalSpy : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_SignalSpy();

private slots:
    void testValid();
    void testCount();

private:
    QQmlEngine engine;
};

tst_SignalSpy::tst_SignalSpy()
{
    qmlRegisterType<MyPropertyMap>("MyImport", 1, 0, "MyPropertyMap");
}

void tst_SignalSpy::testValid()
{
    QQuickView window;
    window.setSource(testFileUrl("signalspy.qml"));
    QVERIFY(window.rootObject() != 0);

    QObject *mouseSpy = window.rootObject()->findChild<QObject*>("mouseSpy");
    QVERIFY(mouseSpy->property("valid").toBool());

    QObject *propertyMapSpy = window.rootObject()->findChild<QObject*>("propertyMapSpy");
    QVERIFY(propertyMapSpy->property("valid").toBool());
}

void tst_SignalSpy::testCount()
{
    QQuickView window;
    window.resize(200, 200);
    window.setSource(testFileUrl("signalspy.qml"));
    window.show();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(window.rootObject() != 0);

    QObject *mouseSpy = window.rootObject()->findChild<QObject*>("mouseSpy");
    QCOMPARE(mouseSpy->property("count").toInt(), 0);

    QObject *propertyMapSpy = window.rootObject()->findChild<QObject*>("propertyMapSpy");
    QCOMPARE(propertyMapSpy->property("count").toInt(), 0);

    QTest::mouseClick(&window, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(100, 100));
    QTRY_COMPARE(mouseSpy->property("count").toInt(), 1);

    MyPropertyMap *propertyMap = static_cast<MyPropertyMap *>(window.rootObject()->findChild<QObject*>("propertyMap"));
    Q_EMIT propertyMap->mySignal();
    QCOMPARE(propertyMapSpy->property("count").toInt(), 1);
}

QTEST_MAIN(tst_SignalSpy)

#include "tst_signalspy.moc"
