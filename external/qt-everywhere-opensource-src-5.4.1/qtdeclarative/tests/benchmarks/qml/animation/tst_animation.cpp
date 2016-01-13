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
#include <private/qqmlmetatype_p.h>
#include <private/qquickanimation_p_p.h>
#include <QQmlContext>

class tst_animation : public QObject
{
    Q_OBJECT
public:
    tst_animation();

private slots:
    void abstractAnimation();
    void bulkValueAnimator();
    void propertyUpdater();

    void animationtree_qml();

    void animationelements_data();
    void animationelements();

    void numberAnimation();
    void numberAnimationStarted();
    void numberAnimationMultipleTargets();
    void numberAnimationEmpty();

private:
    QQmlEngine engine;
};

tst_animation::tst_animation()
{
}

inline QUrl TEST_FILE(const QString &filename)
{
    return QUrl::fromLocalFile(QLatin1String(SRCDIR) + QLatin1String("/data/") + filename);
}

void tst_animation::abstractAnimation()
{
    QBENCHMARK {
        QAbstractAnimationJob *animation = new QAbstractAnimationJob;
        delete animation;
    }
}

void tst_animation::bulkValueAnimator()
{
    QBENCHMARK {
        QQuickBulkValueAnimator *animator = new QQuickBulkValueAnimator;
        delete animator;
    }
}

void tst_animation::propertyUpdater()
{
    QBENCHMARK {
        QQuickAnimationPropertyUpdater *updater = new QQuickAnimationPropertyUpdater;
        delete updater;
    }
}

void tst_animation::animationtree_qml()
{
    QQmlComponent component(&engine, TEST_FILE("animation.qml"));
    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_animation::animationelements_data()
{
    QTest::addColumn<QString>("type");

    QSet<QString> types = QQmlMetaType::qmlTypeNames().toSet();
    foreach (const QString &type, types) {
        if (type.contains(QLatin1String("Animation")))
            QTest::newRow(type.toLatin1()) << type;
    }

    QTest::newRow("QtQuick/Behavior") << "QtQuick/Behavior";
    QTest::newRow("QtQuick/Transition") << "QtQuick/Transition";
}

void tst_animation::animationelements()
{
    QFETCH(QString, type);
    QQmlType *t = QQmlMetaType::qmlType(type, 2, 0);
    if (!t || !t->isCreatable())
        QSKIP("Non-creatable type");

    QBENCHMARK {
        QObject *obj = t->create();
        delete obj;
    }
}

void tst_animation::numberAnimation()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem { Rectangle { id: rect; NumberAnimation { target: rect; property: \"x\"; to: 100; duration: 500; easing.type: Easing.InOutQuad } } }", QUrl());

    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_animation::numberAnimationStarted()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem { Rectangle { id: rect; NumberAnimation { target: rect; property: \"x\"; to: 100; duration: 500; easing.type: Easing.InOutQuad; running: true; paused: true } } }", QUrl());

    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_animation::numberAnimationMultipleTargets()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem { Rectangle { id: rect; NumberAnimation { target: rect; properties: \"x,y,z,width,height,implicitWidth,implicitHeight\"; to: 100; duration: 500; easing.type: Easing.InOutQuad; running: true; paused: true } } }", QUrl());

    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_animation::numberAnimationEmpty()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nNumberAnimation { }", QUrl());

    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

QTEST_MAIN(tst_animation)

#include "tst_animation.moc"
