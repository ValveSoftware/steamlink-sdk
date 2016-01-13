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
    QVERIFY(obj->side() == QQuickFlipable::Front);
    obj->setProperty("flipped", QVariant(true));
    QTRY_VERIFY(obj->side() == QQuickFlipable::Back);
    QTRY_VERIFY(obj->side() == QQuickFlipable::Front);
    QTRY_VERIFY(obj->side() == QQuickFlipable::Back);
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
