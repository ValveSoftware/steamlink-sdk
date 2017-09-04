/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "../../shared/util.h"

#include <qtest.h>
#include <QSignalSpy>
#include <QDebug>

#include <Qt3DQuick/qqmlaspectengine.h>

#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qnode.h>
#include <Qt3DCore/private/qnode_p.h>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>

using namespace Qt3DCore::Quick;
using namespace Qt3DCore;

class tst_dynamicnodecreation : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void createSingleEntityViaQmlEngine();
    void createSingleEntityViaAspectEngine();
    void createMultipleEntitiesViaAspectEngine();
    void createEntityAndDynamicChild();
};

void tst_dynamicnodecreation::createSingleEntityViaQmlEngine()
{
    // GIVEN
    QQmlAspectEngine engine;

    // WHEN
    QQmlEngine *qmlEngine = engine.qmlEngine();

    // THEN
    QVERIFY(qmlEngine != nullptr);

    // WHEN
    QQmlComponent component(qmlEngine, testFileUrl("createSingle.qml"));
    auto entity = qobject_cast<QEntity *>(component.create());

    // THEN
    QVERIFY(entity != nullptr);
    QVERIFY(entity->parent() == nullptr);
    QCOMPARE(entity->property("success").toBool(), true);
    QCOMPARE(entity->property("value").toInt(), 12345);

    // WHEN
    auto nodeD = Qt3DCore::QNodePrivate::get(entity);

    // THEN
    QVERIFY(nodeD->m_scene == nullptr);
    QVERIFY(nodeD->m_changeArbiter == nullptr);
    QCOMPARE(nodeD->m_id.isNull(), false);
}

void tst_dynamicnodecreation::createSingleEntityViaAspectEngine()
{
    // GIVEN
    QQmlAspectEngine engine;

    // WHEN
    engine.setSource(testFileUrl("createSingle.qml"));
    auto entity = engine.aspectEngine()->rootEntity().data();

    // THEN
    QVERIFY(entity != nullptr);
    QVERIFY(entity->parent() == engine.aspectEngine());
    QCOMPARE(entity->property("success").toBool(), true);
    QCOMPARE(entity->property("value").toInt(), 12345);

    // WHEN
    auto nodeD = QNodePrivate::get(entity);

    // THEN
    QVERIFY(nodeD->m_scene != nullptr);
    QVERIFY(nodeD->m_changeArbiter != nullptr);
    QCOMPARE(nodeD->m_id.isNull(), false);
}

void tst_dynamicnodecreation::createMultipleEntitiesViaAspectEngine()
{
    // GIVEN
    QQmlAspectEngine engine;

    // WHEN
    engine.setSource(testFileUrl("createMultiple.qml"));
    auto entity = engine.aspectEngine()->rootEntity().data();

    // THEN
    QVERIFY(entity != nullptr);
    QVERIFY(entity->parent() == engine.aspectEngine());
    QCOMPARE(entity->property("success").toBool(), true);
    QCOMPARE(entity->property("value").toInt(), 12345);
    QCOMPARE(entity->childNodes().size(), 1);

    // WHEN
    auto nodeD = Qt3DCore::QNodePrivate::get(entity);

    // THEN
    QVERIFY(nodeD->m_scene != nullptr);
    QVERIFY(nodeD->m_changeArbiter != nullptr);
    QCOMPARE(nodeD->m_id.isNull(), false);

    // WHEN
    auto child = qobject_cast<QEntity *>(entity->childNodes().first());

    // THEN
    QCOMPARE(child->objectName(), QLatin1String("childEntity"));
    QCOMPARE(child->property("success").toBool(), false);
    QCOMPARE(child->property("value").toInt(), 54321);
    QCOMPARE(child->childNodes().size(), 0);
    QCOMPARE(child->parentEntity(), entity);
}

void tst_dynamicnodecreation::createEntityAndDynamicChild()
{
    // GIVEN
    QQmlAspectEngine engine;

    // WHEN
    engine.setSource(testFileUrl("createDynamicChild.qml"));
    auto entity = engine.aspectEngine()->rootEntity().data();

    // THEN
    QVERIFY(entity != nullptr);
    QVERIFY(entity->parent() == engine.aspectEngine());
    QCOMPARE(entity->property("success").toBool(), true);
    QCOMPARE(entity->property("value").toInt(), 12345);
    QCOMPARE(entity->childNodes().size(), 0);

    // WHEN
    auto *nodeD = Qt3DCore::QNodePrivate::get(entity);

    // THEN
    QVERIFY(nodeD->m_scene != nullptr);
    QVERIFY(nodeD->m_changeArbiter != nullptr);
    QCOMPARE(nodeD->m_id.isNull(), false);

    // WHEN
    //QGuiApplication::processEvents(QEventLoop::AllEvents, 10);
    QGuiApplication::processEvents();
    auto child = qobject_cast<QEntity *>(entity->childNodes().first());

    // THEN
    QCOMPARE(child->objectName(), QLatin1String("dynamicChildEntity"));
    QCOMPARE(child->property("success").toBool(), false);
    QCOMPARE(child->property("value").toInt(), 27182818);
    QCOMPARE(child->childNodes().size(), 0);
    QCOMPARE(child->parentEntity(), entity);

    // WHEN
    auto *childD = Qt3DCore::QNodePrivate::get(child);

    // THEN
    QCOMPARE(childD->m_scene, nodeD->m_scene);
    QCOMPARE(childD->m_changeArbiter, nodeD->m_changeArbiter);
    QCOMPARE(childD->m_id.isNull(), false);
}

QTEST_MAIN(tst_dynamicnodecreation)

#include "tst_dynamicnodecreation.moc"
