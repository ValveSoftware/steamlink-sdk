/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QtTest/QtTest>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/private/qentity_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qcomponent.h>
#include <QtCore/qscopedpointer.h>

using namespace Qt3DCore;

class tst_Entity : public QObject
{
    Q_OBJECT
public:
    tst_Entity() : QObject()
    {
        qRegisterMetaType<Qt3DCore::QNode*>();
    }
    ~tst_Entity() {}

private slots:
    void constructionDestruction();

    void addComponentSingleParentSingleAggregation();
    void addComponentSingleParentSeveralAggregations();
    void addComponentsSeveralParentsSingleAggregations();
    void addComponentsSeveralParentsSeveralAggregations();

    void removeComponentSingleParentSingleAggregation();
    void removeComponentSingleParentSeveralAggregations();
    void removeComponentsSeveralParentsSingleAggreation();
    void removeComponentsSeveralParentsSeveralAggregations();

    void addSeveralTimesSameComponent();
    void removeSeveralTimesSameComponent();

    void checkCloning_data();
    void checkCloning();

    void checkComponentBookkeeping();
};

class MyQComponent : public Qt3DCore::QComponent
{
    Q_OBJECT
public:
    explicit MyQComponent(Qt3DCore::QNode *parent = 0)
        : QComponent(parent)
    {}
};


class MyEntity : public Qt3DCore::QEntity
{
public:
    explicit MyEntity(Qt3DCore::QNode *parent = 0)
        : QEntity(parent)
    {}
};

void tst_Entity::constructionDestruction()
{
    // GIVEN
    QEntity *entity = nullptr;
    // WHEN
    entity = new QEntity;
    // THEN
    QVERIFY(entity != nullptr);

    delete entity;

    // GIVEN
    QScopedPointer<QEntity> entity2(new QEntity);
    // WHEN
    entity2.reset(nullptr);
    // THEN
    // this should not crash
}

void tst_Entity::addComponentSingleParentSingleAggregation()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity(new QEntity());
    MyQComponent *comp = new MyQComponent(entity.data());
    QCoreApplication::processEvents();

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 0);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 0);

    // WHEN
    entity->addComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 1);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 1);
}

void tst_Entity::addComponentSingleParentSeveralAggregations()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity1(new QEntity());
    QScopedPointer<Qt3DCore::QEntity> entity2(new QEntity());

    MyQComponent *comp1 = new MyQComponent(entity1.data());
    MyQComponent *comp2 = new MyQComponent(entity1.data());
    MyQComponent *comp3 = new MyQComponent(entity1.data());
    QCoreApplication::processEvents();

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity1.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 0);

    QCOMPARE(entity1->children().size(), 3);
    QCOMPARE(entity2->children().size(), 0);

    QCOMPARE(comp1->entities().size(), 0);
    QCOMPARE(comp2->entities().size(), 0);
    QCOMPARE(comp3->entities().size(), 0);

    // WHEN
    entity1->addComponent(comp1);
    entity1->addComponent(comp2);
    entity1->addComponent(comp3);

    entity2->addComponent(comp1);
    entity2->addComponent(comp2);
    entity2->addComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity1.data());

    QCOMPARE(entity1->components().size(), 3);
    QCOMPARE(entity2->components().size(), 3);

    QCOMPARE(entity1->children().size(), 3);
    QCOMPARE(entity2->children().size(), 0);

    QCOMPARE(comp1->entities().size(), 2);
    QCOMPARE(comp2->entities().size(), 2);
    QCOMPARE(comp3->entities().size(), 2);
}

void tst_Entity::addComponentsSeveralParentsSingleAggregations()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity1(new QEntity());
    QScopedPointer<Qt3DCore::QEntity> entity2(new QEntity());

    MyQComponent *comp1 = new MyQComponent(entity1.data());
    MyQComponent *comp2 = new MyQComponent(entity1.data());
    MyQComponent *comp3 = new MyQComponent(entity2.data());
    QCoreApplication::processEvents();

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 0);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 0);
    QCOMPARE(comp2->entities().size(), 0);
    QCOMPARE(comp3->entities().size(), 0);

    // WHEN
    entity1->addComponent(comp1);
    entity1->addComponent(comp2);

    entity2->addComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 2);
    QCOMPARE(entity2->components().size(), 1);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 1);
    QCOMPARE(comp2->entities().size(), 1);
    QCOMPARE(comp3->entities().size(), 1);
}

void tst_Entity::addComponentsSeveralParentsSeveralAggregations()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity1(new QEntity());
    QScopedPointer<Qt3DCore::QEntity> entity2(new QEntity());

    MyQComponent *comp1 = new MyQComponent(entity1.data());
    MyQComponent *comp2 = new MyQComponent(entity1.data());
    MyQComponent *comp3 = new MyQComponent(entity2.data());
    QCoreApplication::processEvents();

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 0);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 0);
    QCOMPARE(comp2->entities().size(), 0);
    QCOMPARE(comp3->entities().size(), 0);

    // WHEN
    entity1->addComponent(comp1);
    entity1->addComponent(comp2);
    entity1->addComponent(comp3);

    entity2->addComponent(comp1);
    entity2->addComponent(comp2);
    entity2->addComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 3);
    QCOMPARE(entity2->components().size(), 3);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 2);
    QCOMPARE(comp2->entities().size(), 2);
    QCOMPARE(comp3->entities().size(), 2);
}

void tst_Entity::removeComponentSingleParentSingleAggregation()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity(new QEntity());
    MyQComponent *comp = new MyQComponent(entity.data());
    QCoreApplication::processEvents();
    entity->addComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 1);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 1);

    // WHEN
    entity->removeComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 0);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 0);
}

void tst_Entity::removeComponentSingleParentSeveralAggregations()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity1(new QEntity());
    QScopedPointer<Qt3DCore::QEntity> entity2(new QEntity());

    MyQComponent *comp1 = new MyQComponent(entity1.data());
    MyQComponent *comp2 = new MyQComponent(entity1.data());
    MyQComponent *comp3 = new MyQComponent(entity1.data());
    QCoreApplication::processEvents();

    entity1->addComponent(comp1);
    entity1->addComponent(comp2);
    entity1->addComponent(comp3);

    entity2->addComponent(comp1);
    entity2->addComponent(comp2);
    entity2->addComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity1.data());

    QCOMPARE(entity1->components().size(), 3);
    QCOMPARE(entity2->components().size(), 3);

    QCOMPARE(entity1->children().size(), 3);
    QCOMPARE(entity2->children().size(), 0);

    QCOMPARE(comp1->entities().size(), 2);
    QCOMPARE(comp2->entities().size(), 2);
    QCOMPARE(comp3->entities().size(), 2);

    // WHEN
    entity1->removeComponent(comp1);
    entity1->removeComponent(comp2);
    entity1->removeComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity1.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 3);

    QCOMPARE(entity1->children().size(), 3);
    QCOMPARE(entity2->children().size(), 0);

    QCOMPARE(comp1->entities().size(), 1);
    QCOMPARE(comp2->entities().size(), 1);
    QCOMPARE(comp3->entities().size(), 1);

    // WHEN
    entity2->removeComponent(comp1);
    entity2->removeComponent(comp2);
    entity2->removeComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity1.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 0);

    QCOMPARE(entity1->children().size(), 3);
    QCOMPARE(entity2->children().size(), 0);

    QCOMPARE(comp1->entities().size(), 0);
    QCOMPARE(comp2->entities().size(), 0);
    QCOMPARE(comp3->entities().size(), 0);
}

void tst_Entity::removeComponentsSeveralParentsSingleAggreation()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity1(new QEntity());
    QScopedPointer<Qt3DCore::QEntity> entity2(new QEntity());

    MyQComponent *comp1 = new MyQComponent(entity1.data());
    MyQComponent *comp2 = new MyQComponent(entity1.data());
    MyQComponent *comp3 = new MyQComponent(entity2.data());
    QCoreApplication::processEvents();

    // WHEN
    entity1->addComponent(comp1);
    entity1->addComponent(comp2);
    entity2->addComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 2);
    QCOMPARE(entity2->components().size(), 1);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 1);
    QCOMPARE(comp2->entities().size(), 1);
    QCOMPARE(comp3->entities().size(), 1);

    // WHEN
    entity1->removeComponent(comp1);
    entity1->removeComponent(comp2);
    entity2->removeComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 0);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 0);
    QCOMPARE(comp2->entities().size(), 0);
    QCOMPARE(comp3->entities().size(), 0);
}

void tst_Entity::removeComponentsSeveralParentsSeveralAggregations()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity1(new QEntity());
    QScopedPointer<Qt3DCore::QEntity> entity2(new QEntity());

    MyQComponent *comp1 = new MyQComponent(entity1.data());
    MyQComponent *comp2 = new MyQComponent(entity1.data());
    MyQComponent *comp3 = new MyQComponent(entity2.data());
    QCoreApplication::processEvents();

    // WHEN
    entity1->addComponent(comp1);
    entity1->addComponent(comp2);
    entity1->addComponent(comp3);

    entity2->addComponent(comp1);
    entity2->addComponent(comp2);
    entity2->addComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 3);
    QCOMPARE(entity2->components().size(), 3);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 2);
    QCOMPARE(comp2->entities().size(), 2);
    QCOMPARE(comp3->entities().size(), 2);

    // WHEN
    entity1->removeComponent(comp1);
    entity1->removeComponent(comp2);
    entity1->removeComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 3);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 1);
    QCOMPARE(comp2->entities().size(), 1);
    QCOMPARE(comp3->entities().size(), 1);

    // WHEN
    entity2->removeComponent(comp1);
    entity2->removeComponent(comp2);
    entity2->removeComponent(comp3);

    // THEN
    QVERIFY(comp1->parent() == entity1.data());
    QVERIFY(comp2->parent() == entity1.data());
    QVERIFY(comp3->parent() == entity2.data());

    QCOMPARE(entity1->components().size(), 0);
    QCOMPARE(entity2->components().size(), 0);

    QCOMPARE(entity1->children().size(), 2);
    QCOMPARE(entity2->children().size(), 1);

    QCOMPARE(comp1->entities().size(), 0);
    QCOMPARE(comp2->entities().size(), 0);
    QCOMPARE(comp3->entities().size(), 0);
}

void tst_Entity::addSeveralTimesSameComponent()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity(new QEntity());
    MyQComponent *comp = new MyQComponent(entity.data());
    QCoreApplication::processEvents();
    entity->addComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 1);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 1);

    // WHEN
    entity->addComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 1);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 1);
}

void tst_Entity::removeSeveralTimesSameComponent()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> entity(new QEntity());
    MyQComponent *comp = new MyQComponent(entity.data());
    QCoreApplication::processEvents();
    entity->addComponent(comp);
    entity->removeComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 0);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 0);

    // WHEN
    entity->removeComponent(comp);

    // THEN
    QVERIFY(comp->parent() == entity.data());
    QCOMPARE(entity->components().size(), 0);
    QCOMPARE(entity->children().size(), 1);
    QCOMPARE(comp->entities().size(), 0);
}

void tst_Entity::checkCloning_data()
{
    QTest::addColumn<Qt3DCore::QEntity *>("entity");

    QTest::newRow("defaultConstructed") << new MyEntity();

    Qt3DCore::QEntity *entityWithComponents = new MyEntity();
    Qt3DCore::QComponent *component1 = new MyQComponent();
    Qt3DCore::QComponent *component2 = new MyQComponent();
    Qt3DCore::QComponent *component3 = new MyQComponent();
    entityWithComponents->addComponent(component1);
    entityWithComponents->addComponent(component2);
    entityWithComponents->addComponent(component3);
    QTest::newRow("entityWithComponents") << entityWithComponents;
}

void tst_Entity::checkCloning()
{
    // GIVEN
    QFETCH(Qt3DCore::QEntity *, entity);

    // WHEN
    Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(entity);
    QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

    // THEN
    QCOMPARE(creationChanges.size(), 1 + entity->components().size());

    const Qt3DCore::QNodeCreatedChangePtr<Qt3DCore::QEntityData> creationChangeData =
            qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DCore::QEntityData>>(creationChanges.first());
    const Qt3DCore::QEntityData &cloneData = creationChangeData->data;

    // THEN
    QCOMPARE(creationChangeData->subjectId(), entity->id());
    QCOMPARE(creationChangeData->isNodeEnabled(), entity->isEnabled());
    QCOMPARE(creationChangeData->metaObject(), entity->metaObject());
    QCOMPARE(creationChangeData->parentId(), entity->parentNode() ? entity->parentNode()->id() : Qt3DCore::QNodeId());
    QCOMPARE(cloneData.parentEntityId, entity->parentEntity() ? entity->parentEntity()->id() : Qt3DCore::QNodeId());
    QCOMPARE(cloneData.componentIdsAndTypes.size(), entity->components().size());

    const QVector<Qt3DCore::QComponent *> &components = entity->components();
    for (int i = 0, m = components.size(); i < m; ++i) {
        QCOMPARE(cloneData.componentIdsAndTypes.at(i).id, components.at(i)->id());
        QCOMPARE(cloneData.componentIdsAndTypes.at(i).type, components.at(i)->metaObject());
    }
}

void tst_Entity::checkComponentBookkeeping()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QEntity> rootEntity(new Qt3DCore::QEntity);
    {
        // WHEN
        QScopedPointer<Qt3DCore::QComponent> comp(new MyQComponent(rootEntity.data()));
        rootEntity->addComponent(comp.data());

        // THEN
        QCOMPARE(comp->parent(), rootEntity.data());
        QCOMPARE(rootEntity->components().size(), 1);
    }
    // THEN (Should not crash and comp should be automatically removed)
    QVERIFY(rootEntity->components().empty());

    {
        // WHEN
        QScopedPointer<Qt3DCore::QEntity> someOtherEntity(new Qt3DCore::QEntity);
        QScopedPointer<Qt3DCore::QComponent> comp(new MyQComponent(someOtherEntity.data()));
        rootEntity->addComponent(comp.data());

        // THEN
        QCOMPARE(comp->parent(), someOtherEntity.data());
        QCOMPARE(rootEntity->components().size(), 1);

        // WHEN
        rootEntity.reset();
        comp.reset();

        // THEN (Should not crash when the comp is destroyed (tests for failed removal of destruction helper)
    }
}

Qt3DCore::QNodeId parentEntityId(Qt3DCore::QEntity *entity)
{
    Qt3DCore::QEntityPrivate *d = static_cast<Qt3DCore::QEntityPrivate*>(Qt3DCore::QNodePrivate::get(entity));
    return d->parentEntityId();
}

QTEST_MAIN(tst_Entity)

#include "tst_qentity.moc"
