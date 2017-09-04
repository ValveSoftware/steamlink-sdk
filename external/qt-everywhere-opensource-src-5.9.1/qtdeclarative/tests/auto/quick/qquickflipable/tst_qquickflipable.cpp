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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <private/qquickflipable_p.h>
#include <private/qqmlvaluetype_p.h>
#include <QFontMetrics>
#include <QtQuick/private/qquickrectangle_p.h>
#include <math.h>
#include "../../shared/util.h"

class tst_qquickflipable : public QQmlDataTest
{
    Q_OBJECT
public:

private slots:
    void create();
    void checkFrontAndBack();
    void setFrontAndBack();
    void flipFlipable();

    // below here task issues
    void QTBUG_9161_crash();
    void QTBUG_8474_qgv_abort();

private:
    QQmlEngine engine;
};

void tst_qquickflipable::create()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-flipable.qml"));
    QQuickFlipable *obj = qobject_cast<QQuickFlipable*>(c.create());

    QVERIFY(obj != 0);
    delete obj;
}

void tst_qquickflipable::checkFrontAndBack()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-flipable.qml"));
    QQuickFlipable *obj = qobject_cast<QQuickFlipable*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->front() != 0);
    QVERIFY(obj->back() != 0);
    delete obj;
}

void tst_qquickflipable::setFrontAndBack()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-flipable.qml"));
    QQuickFlipable *obj = qobject_cast<QQuickFlipable*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->front() != 0);
    QVERIFY(obj->back() != 0);

    QString message = c.url().toString() + ":3:1: QML Flipable: front is a write-once property";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    obj->setFront(new QQuickRectangle());

    message = c.url().toString() + ":3:1: QML Flipable: back is a write-once property";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    obj->setBack(new QQuickRectangle());
    delete obj;
}

void tst_qquickflipable::flipFlipable()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("flip-flipable.qml"));
    QQuickFlipable *obj = qobject_cast<QQuickFlipable*>(c.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->side(), QQuickFlipable::Front);
    obj->setProperty("flipped", QVariant(true));
    QTRY_COMPARE(obj->side(), QQuickFlipable::Back);
    QTRY_COMPARE(obj->side(), QQuickFlipable::Front);
    QTRY_COMPARE(obj->side(), QQuickFlipable::Back);
    delete obj;
}

void tst_qquickflipable::QTBUG_9161_crash()
{
    QQuickView *window = new QQuickView;
    window->setSource(testFileUrl("crash.qml"));
    QQuickItem *root = window->rootObject();
    QVERIFY(root != 0);
    window->show();
    delete window;
}

void tst_qquickflipable::QTBUG_8474_qgv_abort()
{
    QQuickView *window = new QQuickView;
    window->setSource(testFileUrl("flipable-abort.qml"));
    QQuickItem *root = window->rootObject();
    QVERIFY(root != 0);
    window->show();
    delete window;
}

QTEST_MAIN(tst_qquickflipable)

#include "tst_qquickflipable.moc"
