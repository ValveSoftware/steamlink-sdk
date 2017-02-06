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
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qnode.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qcomponent.h>
#include <Qt3DCore/private/qobservableinterface_p.h>
#include <Qt3DCore/private/qlockableobserverinterface_p.h>
#include <private/qnode_p.h>

class tst_QScene : public QObject
{
    Q_OBJECT
public:
    tst_QScene() : QObject() {}
    ~tst_QScene() {}

private slots:
    void addObservable();
    void addNodeObservable();
    void removeObservable();
    void removeNodeObservable();
    void addChildNode();
    void removeChildNode();
    void addEntityForComponent();
    void removeEntityForComponent();
    void hasEntityForComponent();
};

class tst_LockableObserver : public Qt3DCore::QLockableObserverInterface
{
public:
    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &) Q_DECL_OVERRIDE {}
    void sceneChangeEventWithLock(const Qt3DCore::QSceneChangePtr &) Q_DECL_OVERRIDE {}
    void sceneChangeEventWithLock(const Qt3DCore::QSceneChangeList &) Q_DECL_OVERRIDE {}
};

class tst_Observable : public Qt3DCore::QObservableInterface
{
public:
    void setArbiter(Qt3DCore::QLockableObserverInterface *observer)
    {
        m_arbiter = observer;
    }

protected:
    void notifyObservers(const Qt3DCore::QSceneChangePtr &) {}

private:
    Qt3DCore::QLockableObserverInterface *m_arbiter;
};

class tst_Node : public Qt3DCore::QNode
{
    Q_OBJECT
public:
    tst_Node() : Qt3DCore::QNode()
    {}
};

class tst_Component : public Qt3DCore::QComponent
{
    Q_OBJECT
public:
    tst_Component() : Qt3DCore::QComponent()
    {}
};

void tst_QScene::addObservable()
{
    // GIVEN
    Qt3DCore::QNode *node1 = new tst_Node();
    Qt3DCore::QNode *node2 = new tst_Node();

    QList<tst_Observable *> observables;

    for (int i = 0; i < 10; i++)
        observables.append(new tst_Observable());

    Qt3DCore::QScene *scene = new Qt3DCore::QScene;
    scene->setArbiter(new tst_LockableObserver);

    // WHEN
    for (int i = 0; i < 5; i++)
        scene->addObservable(observables.at(i), node1->id());

    for (int i = 0; i < 5; i++)
        scene->addObservable(observables.at(i + 5), node2->id());

    Qt3DCore::QObservableList obs1 = scene->lookupObservables(node1->id());
    Qt3DCore::QObservableList obs2 = scene->lookupObservables(node2->id());

    // THEN
    QCOMPARE(obs1.count(), 5);
    QCOMPARE(obs2.count(), obs1.count());

    Q_FOREACH (Qt3DCore::QObservableInterface *o, obs1) {
        QVERIFY(scene->nodeIdFromObservable(o) == node1->id());
        QVERIFY(scene->lookupNode(node1->id()) == nullptr);
    }
    Q_FOREACH (Qt3DCore::QObservableInterface *o, obs2) {
        QVERIFY(scene->nodeIdFromObservable(o) == node2->id());
        QVERIFY(scene->lookupNode(node2->id()) == nullptr);
    }
}

void tst_QScene::addNodeObservable()
{
    // GIBEN
    QList<Qt3DCore::QNode *> nodes;

    for (int i = 0; i < 10; i++)
        nodes.append(new tst_Node());

    Qt3DCore::QScene *scene = new Qt3DCore::QScene;
    scene->setArbiter(new tst_LockableObserver);

    // WHEN
    for (int i = 0; i < 10; i++)
        scene->addObservable(nodes.at(i));

    // THEN
    Q_FOREACH (Qt3DCore::QNode *n, nodes) {
        QVERIFY(n == scene->lookupNode(n->id()));
        QVERIFY(scene->lookupObservables(n->id()).isEmpty());
    }
}

void tst_QScene::removeObservable()
{
    // GIVEN
    Qt3DCore::QNode *node1 = new tst_Node();
    Qt3DCore::QNode *node2 = new tst_Node();

    QList<tst_Observable *> observables;

    for (int i = 0; i < 10; i++)
        observables.append(new tst_Observable());

    Qt3DCore::QScene *scene = new Qt3DCore::QScene;
    scene->setArbiter(new tst_LockableObserver);

    // WHEN
    for (int i = 0; i < 5; i++)
        scene->addObservable(observables.at(i), node1->id());

    for (int i = 0; i < 5; i++)
        scene->addObservable(observables.at(i + 5), node2->id());

    Qt3DCore::QObservableList obs1 = scene->lookupObservables(node1->id());
    Qt3DCore::QObservableList obs2 = scene->lookupObservables(node2->id());

    // THEN
    QCOMPARE(obs1.count(), 5);
    QCOMPARE(obs2.count(), obs1.count());

    // WHEN
    scene->removeObservable(observables.at(0), node1->id());
    // THEN
    QCOMPARE(scene->lookupObservables(node1->id()).count(), 4);

    // WHEN
    scene->removeObservable(observables.at(0), node1->id());
    // THEN
    QCOMPARE(scene->lookupObservables(node1->id()).count(), 4);

    // WHEN
    scene->removeObservable(observables.at(6), node1->id());
    // THEN
    QCOMPARE(scene->lookupObservables(node1->id()).count(), 4);
    QCOMPARE(scene->lookupObservables(node2->id()).count(), 5);

    // WHEN
    scene->removeObservable(observables.at(0), node2->id());
    // THEN
    QCOMPARE(scene->lookupObservables(node2->id()).count(), 5);
    QVERIFY(scene->nodeIdFromObservable(observables.at(0)) == Qt3DCore::QNodeId());
}

void tst_QScene::removeNodeObservable()
{
    // GIVEN
    Qt3DCore::QNode *node1 = new tst_Node();
    Qt3DCore::QNode *node2 = new tst_Node();

    QList<tst_Observable *> observables;

    for (int i = 0; i < 10; i++)
        observables.append(new tst_Observable());

    Qt3DCore::QScene *scene = new Qt3DCore::QScene;
    scene->setArbiter(new tst_LockableObserver);

    // WHEN
    scene->addObservable(node1);
    scene->addObservable(node2);

    for (int i = 0; i < 5; i++)
        scene->addObservable(observables.at(i), node1->id());

    for (int i = 0; i < 5; i++)
        scene->addObservable(observables.at(i + 5), node2->id());

    // THEN
    Qt3DCore::QObservableList obs1 = scene->lookupObservables(node1->id());
    Qt3DCore::QObservableList obs2 = scene->lookupObservables(node2->id());

    QCOMPARE(obs1.count(), 5);
    QCOMPARE(obs2.count(), obs1.count());

    // WHEN
    scene->removeObservable(node1);

    // THEN
    QVERIFY(scene->lookupNode(node1->id()) == nullptr);
    QVERIFY(scene->lookupObservables(node1->id()).empty());
    QVERIFY(scene->nodeIdFromObservable(observables.at(0)) == Qt3DCore::QNodeId());

    QVERIFY(scene->lookupNode(node2->id()) == node2);
    QCOMPARE(scene->lookupObservables(node2->id()).count(), 5);
    QVERIFY(scene->nodeIdFromObservable(observables.at(9)) == node2->id());
}

void tst_QScene::addChildNode()
{
    // GIVEN
    Qt3DCore::QScene *scene = new Qt3DCore::QScene;

    QList<Qt3DCore::QNode *> nodes;

    Qt3DCore::QNode *root = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene);

    // WHEN
    scene->addObservable(root);
    // THEN
    QVERIFY(scene->lookupNode(root->id()) == root);

    // WHEN
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QNode *child = new tst_Node();
        if (nodes.isEmpty())
            child->setParent(root);
        else
            child->setParent(nodes.last());
        nodes.append(child);
    }
    QCoreApplication::processEvents();

    // THEN
    Q_FOREACH (Qt3DCore::QNode *n, nodes) {
        QVERIFY(scene->lookupNode(n->id()) == n);
    }
}

void tst_QScene::removeChildNode()
{
    // GIVEN
    Qt3DCore::QScene *scene = new Qt3DCore::QScene;

    QList<Qt3DCore::QNode *> nodes;

    Qt3DCore::QNode *root = new tst_Node;
    Qt3DCore::QNodePrivate::get(root)->setScene(scene);
    scene->addObservable(root);

    // WHEN
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QNode *child = new tst_Node;
        if (nodes.isEmpty())
            child->setParent(root);
        else
            child->setParent(nodes.last());
        nodes.append(child);
    }

    // THEN
    while (!nodes.isEmpty()) {
        Qt3DCore::QNode *lst = nodes.takeLast();
        QVERIFY(scene->lookupNode(lst->id()) == lst);
        if (lst->parentNode() != nullptr) {
            lst->setParent(Q_NODE_NULLPTR);
            QCoreApplication::processEvents();
            QVERIFY(scene->lookupNode(lst->id()) == nullptr);
        }
    }
}

void tst_QScene::addEntityForComponent()
{
    // GIVEN
    Qt3DCore::QScene *scene = new Qt3DCore::QScene;

    QList<Qt3DCore::QEntity *> entities;
    QList<Qt3DCore::QComponent *> components;

    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *entity = new Qt3DCore::QEntity();
        Qt3DCore::QComponent *comp = new tst_Component();

        Qt3DCore::QNodePrivate::get(entity)->setScene(scene);
        Qt3DCore::QNodePrivate::get(comp)->setScene(scene);
        entities << entity;
        components << comp;
    }

    // WHEN
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *e = entities.at(i);
        for (int j = 0; j < 10; j++) {
            e->addComponent(components.at(j));
        }
    }

    // THEN
    for (int i = 0; i < 10; i++) {
        QVector<Qt3DCore::QNodeId> ids = scene->entitiesForComponent(components.at(i)->id());
        QCOMPARE(ids.count(), 10);
    }
}

void tst_QScene::removeEntityForComponent()
{
    // GIVEN
    Qt3DCore::QScene *scene = new Qt3DCore::QScene;

    QList<Qt3DCore::QEntity *> entities;
    QList<Qt3DCore::QComponent *> components;

    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *entity = new Qt3DCore::QEntity();
        Qt3DCore::QComponent *comp = new tst_Component();

        Qt3DCore::QNodePrivate::get(entity)->setScene(scene);
        Qt3DCore::QNodePrivate::get(comp)->setScene(scene);
        entities << entity;
        components << comp;
    }

    // WHEN
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *e = entities.at(i);
        for (int j = 0; j < 10; j++) {
            e->addComponent(components.at(j));
        }
    }

    // THEN
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *e = entities.at(i);
        for (int j = 0; j < 10; j++) {
            e->removeComponent(components.at(j));
            QCOMPARE(scene->entitiesForComponent(components.at(j)->id()).count(), 10 - (i + 1));
        }
    }
}

void tst_QScene::hasEntityForComponent()
{
    // GIVEN
    Qt3DCore::QScene *scene = new Qt3DCore::QScene;

    QList<Qt3DCore::QEntity *> entities;
    QList<Qt3DCore::QComponent *> components;

    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *entity = new Qt3DCore::QEntity();
        Qt3DCore::QComponent *comp = new tst_Component();

        Qt3DCore::QNodePrivate::get(entity)->setScene(scene);
        Qt3DCore::QNodePrivate::get(comp)->setScene(scene);
        entities << entity;
        components << comp;
    }

    // WHEN
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *e = entities.at(i);
        for (int j = 0; j < 10; j++) {
            e->addComponent(components.at(j));
        }
    }

    // THEN
    for (int i = 0; i < 10; i++)
        QVERIFY(scene->hasEntityForComponent(components.at(i)->id(), entities.at(i)->id()));
}

QTEST_MAIN(tst_QScene)

#include "tst_qscene.moc"
