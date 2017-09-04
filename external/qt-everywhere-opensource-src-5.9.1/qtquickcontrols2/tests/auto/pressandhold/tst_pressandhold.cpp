/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

class tst_PressAndHold : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void pressAndHold_data();
    void pressAndHold();

    void keepSelection_data();
    void keepSelection();
};

void tst_PressAndHold::initTestCase()
{
    QGuiApplication::styleHints()->setMousePressAndHoldInterval(100);
}

void tst_PressAndHold::cleanupTestCase()
{
    QGuiApplication::styleHints()->setMousePressAndHoldInterval(-1);
}

void tst_PressAndHold::pressAndHold_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QByteArray>("signal");

    QTest::newRow("Button") << QByteArray("import QtQuick.Controls 2.1; Button { text: 'Button' }") << QByteArray(SIGNAL(pressAndHold()));
    QTest::newRow("SwipeDelegate") << QByteArray("import QtQuick.Controls 2.1; SwipeDelegate { text: 'SwipeDelegate' }") << QByteArray(SIGNAL(pressAndHold()));
    QTest::newRow("TextField") << QByteArray("import QtQuick.Controls 2.1; TextField { text: 'TextField' }") << QByteArray(SIGNAL(pressAndHold(QQuickMouseEvent*)));
    QTest::newRow("TextArea") << QByteArray("import QtQuick.Controls 2.1; TextArea { text: 'TextArea' }") << QByteArray(SIGNAL(pressAndHold(QQuickMouseEvent*)));
}

void tst_PressAndHold::pressAndHold()
{
    QFETCH(QByteArray, data);
    QFETCH(QByteArray, signal);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(data, QUrl());

    QScopedPointer<QObject> control(component.create());
    QScopedPointer<QObject> waitControl(component.create());
    QVERIFY(!control.isNull() && !waitControl.isNull());

    QSignalSpy spy(control.data(), signal);
    QSignalSpy waitSpy(waitControl.data(), signal);
    QVERIFY(spy.isValid() && waitSpy.isValid());

    int startDragDistance = QGuiApplication::styleHints()->startDragDistance();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent press2(QEvent::MouseButtonPress, QPointF(), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, QPointF(2 * startDragDistance, 2 * startDragDistance), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    // pressAndHold() emitted
    QGuiApplication::sendEvent(control.data(), &press);
    QTRY_COMPARE(spy.count(), 1);
    QGuiApplication::sendEvent(control.data(), &release);
    QCOMPARE(spy.count(), 1);
    spy.clear();

    // pressAndHold() canceled by release
    QGuiApplication::sendEvent(control.data(), &press);
    QGuiApplication::processEvents();
    QGuiApplication::sendEvent(control.data(), &release);
    QCOMPARE(spy.count(), 0);

    // pressAndHold() canceled by move
    QGuiApplication::sendEvent(control.data(), &press);
    QGuiApplication::sendEvent(control.data(), &move); // cancels pressAndHold()
    QGuiApplication::sendEvent(waitControl.data(), &press);
    // by the time the second control emits pressAndHold(), we can reliably
    // assume that the first control would have emitted pressAndHold() if it
    // wasn't canceled as appropriate by the move event above
    QTRY_COMPARE(waitSpy.count(), 1);
    QCOMPARE(spy.count(), 0);
    QGuiApplication::sendEvent(control.data(), &release);
    QGuiApplication::sendEvent(waitControl.data(), &release);
    QCOMPARE(waitSpy.count(), 1);
    QCOMPARE(spy.count(), 0);
    waitSpy.clear();

    // pressAndHold() canceled by 2nd press
    QGuiApplication::sendEvent(control.data(), &press);
    QGuiApplication::sendEvent(control.data(), &press2); // cancels pressAndHold()
    QGuiApplication::sendEvent(waitControl.data(), &press);
    // by the time the second control emits pressAndHold(), we can reliably
    // assume that the first control would have emitted pressAndHold() if it
    // wasn't canceled as appropriate by the 2nd press event above
    QTRY_COMPARE(waitSpy.count(), 1);
    QCOMPARE(spy.count(), 0);
    QGuiApplication::sendEvent(control.data(), &release);
    QGuiApplication::sendEvent(waitControl.data(), &release);
    QCOMPARE(waitSpy.count(), 1);
    QCOMPARE(spy.count(), 0);
    waitSpy.clear();
}

void tst_PressAndHold::keepSelection_data()
{
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("TextField") << QByteArray("import QtQuick.Controls 2.1; TextField { text: 'TextField' }");
    QTest::newRow("TextArea") << QByteArray("import QtQuick.Controls 2.1; TextArea { text: 'TextArea' }");
}

void tst_PressAndHold::keepSelection()
{
    QFETCH(QByteArray, data);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(data, QUrl());

    QScopedPointer<QObject> control(component.create());
    QScopedPointer<QObject> waitControl(component.create());
    QVERIFY(!control.isNull() && !waitControl.isNull());

    QSignalSpy spy(control.data(), SIGNAL(pressAndHold(QQuickMouseEvent*)));
    QSignalSpy waitSpy(waitControl.data(), SIGNAL(pressAndHold(QQuickMouseEvent*)));
    QVERIFY(spy.isValid() && waitSpy.isValid());

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent press2(QEvent::MouseButtonPress, QPointF(), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    QVERIFY(!control->property("text").toString().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(control.data(), "selectAll"));
    QCOMPARE(control->property("selectedText"), control->property("text"));

    // pressAndHold() emitted => selection remains
    QGuiApplication::sendEvent(control.data(), &press);
    QTRY_COMPARE(spy.count(), 1);
    QGuiApplication::sendEvent(control.data(), &release);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(control->property("selectedText"), control->property("text"));
    spy.clear();

    // pressAndHold() canceled by release => selection cleared
    QGuiApplication::sendEvent(control.data(), &press);
    QGuiApplication::processEvents();
    QGuiApplication::sendEvent(control.data(), &release);
    QCOMPARE(spy.count(), 0);
    QVERIFY(control->property("selectedText").toString().isEmpty());

    QVERIFY(QMetaObject::invokeMethod(control.data(), "selectAll"));
    QCOMPARE(control->property("selectedText"), control->property("text"));

    // pressAndHold() canceled by 2nd press => selection cleared
    QGuiApplication::sendEvent(control.data(), &press);
    QGuiApplication::sendEvent(control.data(), &press2); // cancels pressAndHold()
    QGuiApplication::sendEvent(waitControl.data(), &press);
    // by the time the second control emits pressAndHold(), we can reliably
    // assume that the first control would have emitted pressAndHold() if it
    // wasn't canceled as appropriate by the move event above
    QTRY_COMPARE(waitSpy.count(), 1);
    QCOMPARE(spy.count(), 0);
    QGuiApplication::sendEvent(control.data(), &release);
    QGuiApplication::sendEvent(waitControl.data(), &release);
    QCOMPARE(waitSpy.count(), 1);
    QCOMPARE(spy.count(), 0);
    QVERIFY(control->property("selectedText").toString().isEmpty());
    waitSpy.clear();
}

QTEST_MAIN(tst_PressAndHold)

#include "tst_pressandhold.moc"
