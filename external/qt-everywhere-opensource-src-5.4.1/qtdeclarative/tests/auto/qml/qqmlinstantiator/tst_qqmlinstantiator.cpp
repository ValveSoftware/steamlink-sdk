/****************************************************************************
**
** Copyright (C) 2013 Research In Motion.
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
#include <QSignalSpy>
#include <QDebug>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/private/qqmlinstantiator_p.h>
#include <QtQml/qqmlcontext.h>
#include "../../shared/util.h"
#include "stringmodel.h"

class tst_qqmlinstantiator: public QQmlDataTest
{
    Q_OBJECT

private slots:
    void createNone();
    void createSingle();
    void createMultiple();
    void stringModel();
    void activeProperty();
    void intModelChange();
    void createAndRemove();
};

void tst_qqmlinstantiator::createNone()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createNone.qml"));
    QQmlInstantiator *instantiator = qobject_cast<QQmlInstantiator*>(component.create());
    QVERIFY(instantiator != 0);
    QCOMPARE(instantiator->isActive(), true);
    QCOMPARE(instantiator->count(), 0);
    QCOMPARE(instantiator->property("success").toBool(), true);
    QVERIFY(instantiator->delegate()->isReady());
}

void tst_qqmlinstantiator::createSingle()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createSingle.qml"));
    QQmlInstantiator *instantiator = qobject_cast<QQmlInstantiator*>(component.create());
    QVERIFY(instantiator != 0);
    QCOMPARE(instantiator->isActive(), true);
    QCOMPARE(instantiator->count(), 1);
    QVERIFY(instantiator->delegate()->isReady());

    QObject *object = instantiator->object();
    QVERIFY(object);
    QCOMPARE(object->parent(), instantiator);
    QCOMPARE(object->property("success").toBool(), true);
    QCOMPARE(object->property("idx").toInt(), 0);
}

void tst_qqmlinstantiator::createMultiple()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createMultiple.qml"));
    QQmlInstantiator *instantiator = qobject_cast<QQmlInstantiator*>(component.create());
    QVERIFY(instantiator != 0);
    QCOMPARE(instantiator->isActive(), true);
    QCOMPARE(instantiator->count(), 10);

    for (int i=0; i<10; i++) {
        QObject *object = instantiator->objectAt(i);
        QVERIFY(object);
        QCOMPARE(object->parent(), instantiator);
        QCOMPARE(object->property("success").toBool(), true);
        QCOMPARE(object->property("idx").toInt(), i);
    }
}

void tst_qqmlinstantiator::stringModel()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stringModel.qml"));
    QQmlInstantiator *instantiator = qobject_cast<QQmlInstantiator*>(component.create());
    QVERIFY(instantiator != 0);
    QCOMPARE(instantiator->isActive(), true);
    QCOMPARE(instantiator->count(), 4);

    for (int i=0; i<4; i++) {
        QObject *object = instantiator->objectAt(i);
        QVERIFY(object);
        QCOMPARE(object->parent(), instantiator);
        QCOMPARE(object->property("success").toBool(), true);
    }
}

void tst_qqmlinstantiator::activeProperty()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("inactive.qml"));
    QQmlInstantiator *instantiator = qobject_cast<QQmlInstantiator*>(component.create());
    QVERIFY(instantiator != 0);
    QSignalSpy activeSpy(instantiator, SIGNAL(activeChanged()));
    QSignalSpy countSpy(instantiator, SIGNAL(countChanged()));
    QSignalSpy objectSpy(instantiator, SIGNAL(objectChanged()));
    QSignalSpy modelSpy(instantiator, SIGNAL(modelChanged()));
    QCOMPARE(instantiator->isActive(), false);
    QCOMPARE(instantiator->count(), 0);
    QVERIFY(instantiator->delegate()->isReady());

    QCOMPARE(activeSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(objectSpy.count(), 0);
    QCOMPARE(modelSpy.count(), 0);

    instantiator->setActive(true);
    QCOMPARE(instantiator->isActive(), true);
    QCOMPARE(instantiator->count(), 1);

    QCOMPARE(activeSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(objectSpy.count(), 1);
    QCOMPARE(modelSpy.count(), 0);

    QObject *object = instantiator->object();
    QVERIFY(object);
    QCOMPARE(object->parent(), instantiator);
    QCOMPARE(object->property("success").toBool(), true);
    QCOMPARE(object->property("idx").toInt(), 0);
}

void tst_qqmlinstantiator::intModelChange()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createMultiple.qml"));
    QQmlInstantiator *instantiator = qobject_cast<QQmlInstantiator*>(component.create());
    QVERIFY(instantiator != 0);
    QSignalSpy activeSpy(instantiator, SIGNAL(activeChanged()));
    QSignalSpy countSpy(instantiator, SIGNAL(countChanged()));
    QSignalSpy objectSpy(instantiator, SIGNAL(objectChanged()));
    QSignalSpy modelSpy(instantiator, SIGNAL(modelChanged()));
    QCOMPARE(instantiator->count(), 10);

    QCOMPARE(activeSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(objectSpy.count(), 0);
    QCOMPARE(modelSpy.count(), 0);

    instantiator->setModel(QVariant(2));
    QCOMPARE(instantiator->count(), 2);

    QCOMPARE(activeSpy.count(), 0);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(objectSpy.count(), 2);
    QCOMPARE(modelSpy.count(), 1);

    for (int i=0; i<2; i++) {
        QObject *object = instantiator->objectAt(i);
        QVERIFY(object);
        QCOMPARE(object->parent(), instantiator);
        QCOMPARE(object->property("success").toBool(), true);
        QCOMPARE(object->property("idx").toInt(), i);
    }
}

void tst_qqmlinstantiator::createAndRemove()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createAndRemove.qml"));
    StringModel *model = new StringModel("model1");
    engine.rootContext()->setContextProperty("model1", model);
    QObject *rootObject = component.create();
    QVERIFY(rootObject != 0);

    QQmlInstantiator *instantiator =
        qobject_cast<QQmlInstantiator*>(rootObject->findChild<QObject*>("instantiator1"));
    QVERIFY(instantiator != 0);
    model->drop(1);
    QVector<QString> names;
    names << "Beta" << "Gamma" << "Delta";
    for (int i=0; i<3; i++) {
        QObject *object = instantiator->objectAt(i);
        QVERIFY(object);
        QCOMPARE(object->property("datum").toString(), names[i]);
    }
}
QTEST_MAIN(tst_qqmlinstantiator)

#include "tst_qqmlinstantiator.moc"
