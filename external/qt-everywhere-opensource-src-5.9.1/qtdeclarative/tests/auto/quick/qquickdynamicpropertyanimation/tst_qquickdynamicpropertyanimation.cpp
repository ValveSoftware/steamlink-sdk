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
#include <QtTest/QtTest>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlProperty>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QRect>
#include <QtGui/QColor>

#include "../../shared/util.h"

class tst_qquickdynamicpropertyanimation : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickdynamicpropertyanimation() {}

private:
    template<class T>
    void dynamicPropertyAnimation(const QByteArray & propertyName, T toValue)
    {
        QQuickView view(testFileUrl("MyItem.qml"));
        QQuickItem * item = qobject_cast<QQuickItem *>(view.rootObject());
        QVERIFY(item);
        QQmlProperty testProp(item, propertyName);
        QPropertyAnimation animation(item, propertyName, this);
        animation.setEndValue(toValue);
        QCOMPARE(animation.targetObject(), item);
        QCOMPARE(animation.propertyName(), propertyName);
        QCOMPARE(animation.endValue().value<T>(), toValue);
        animation.start();
        QCOMPARE(animation.state(), QAbstractAnimation::Running);
        QTest::qWait(animation.duration());
        QTRY_COMPARE(testProp.read().value<T>(), toValue);
    }

private slots:
    void initTestCase()
    {
        QQmlEngine engine;  // ensure types are registered
        QQmlDataTest::initTestCase();
    }

    void dynamicIntPropertyAnimation();
    void dynamicDoublePropertyAnimation();
    void dynamicRealPropertyAnimation();
    void dynamicPointPropertyAnimation();
    void dynamicSizePropertyAnimation();
    void dynamicRectPropertyAnimation();
    void dynamicColorPropertyAnimation();
    void dynamicVarPropertyAnimation();
};

void tst_qquickdynamicpropertyanimation::dynamicIntPropertyAnimation()
{
    dynamicPropertyAnimation("testInt", 1);
}

void tst_qquickdynamicpropertyanimation::dynamicDoublePropertyAnimation()
{
    dynamicPropertyAnimation("testDouble", 1.0);
}

void tst_qquickdynamicpropertyanimation::dynamicRealPropertyAnimation()
{
    dynamicPropertyAnimation("testReal", qreal(1.0));
}

void tst_qquickdynamicpropertyanimation::dynamicPointPropertyAnimation()
{
    dynamicPropertyAnimation("testPoint", QPoint(1, 1));
}

void tst_qquickdynamicpropertyanimation::dynamicSizePropertyAnimation()
{
    dynamicPropertyAnimation("testSize", QSize(1,1));
}

void tst_qquickdynamicpropertyanimation::dynamicRectPropertyAnimation()
{
    dynamicPropertyAnimation("testRect", QRect(1, 1, 1, 1));
}

void tst_qquickdynamicpropertyanimation::dynamicColorPropertyAnimation()
{
    dynamicPropertyAnimation("testColor", QColor::fromRgbF(1.0, 1.0, 1.0, 1.0));
}

void tst_qquickdynamicpropertyanimation::dynamicVarPropertyAnimation()
{
    dynamicPropertyAnimation("testVar", QVariant::fromValue(1));
}

QTEST_MAIN(tst_qquickdynamicpropertyanimation)

#include "tst_qquickdynamicpropertyanimation.moc"
