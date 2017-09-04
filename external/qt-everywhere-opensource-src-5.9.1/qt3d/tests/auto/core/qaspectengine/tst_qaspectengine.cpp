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
#include <Qt3DCore/QAbstractAspect>
#include <Qt3DCore/qaspectengine.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>

using namespace Qt3DCore;

class PrintRootAspect : public QAbstractAspect
{
    Q_OBJECT
public:
    explicit PrintRootAspect(QObject *parent = 0)
        : QAbstractAspect(parent)
        , m_rootEntityId()
    {
        qDebug() << Q_FUNC_INFO;
    }

private:
    void onRegistered() Q_DECL_OVERRIDE
    {
        qDebug() << Q_FUNC_INFO;
    }

    void onEngineStartup() Q_DECL_OVERRIDE
    {
        qDebug() << Q_FUNC_INFO;
        m_rootEntityId = rootEntityId();
    }

    void onEngineShutdown() Q_DECL_OVERRIDE
    {
        qDebug() << Q_FUNC_INFO;
    }

    QVector<QAspectJobPtr> jobsToExecute(qint64) Q_DECL_OVERRIDE \
    {
        if (m_rootEntityId)
            qDebug() << Q_FUNC_INFO << m_rootEntityId;
        return QVector<QAspectJobPtr>();
    }

    QNodeId m_rootEntityId;
};

#define FAKE_ASPECT(ClassName) \
class ClassName : public QAbstractAspect \
{ \
    Q_OBJECT \
public: \
    explicit ClassName(QObject *parent = 0) \
        : QAbstractAspect(parent) {} \
    \
private: \
    void onRegistered() Q_DECL_OVERRIDE {} \
    void onEngineStartup() Q_DECL_OVERRIDE {} \
    void onEngineShutdown() Q_DECL_OVERRIDE {} \
    \
    QVector<QAspectJobPtr> jobsToExecute(qint64) Q_DECL_OVERRIDE \
    { \
        return QVector<QAspectJobPtr>(); \
    } \
    \
    QVariant executeCommand(const QStringList &args) Q_DECL_OVERRIDE \
    { \
        if (args.size() >= 2 && args.first() == QLatin1Literal("echo")) { \
            QStringList list = args; \
            list.removeFirst(); \
            return QString("%1 said '%2'").arg(metaObject()->className()).arg(list.join(QLatin1Char(' '))); \
        } \
        \
        return QVariant(); \
    } \
};

FAKE_ASPECT(FakeAspect)
FAKE_ASPECT(FakeAspect2)
FAKE_ASPECT(FakeAspect3)

QT3D_REGISTER_ASPECT("fake", FakeAspect)
QT3D_REGISTER_ASPECT("otherfake", FakeAspect2)


class tst_QAspectEngine : public QObject
{
    Q_OBJECT
public:
    tst_QAspectEngine() : QObject() {}
    ~tst_QAspectEngine() {}

private Q_SLOTS:
    // TODO: Add more QAspectEngine tests

    void constructionDestruction()
    {
        QAspectEngine *engine = new QAspectEngine;
        QVERIFY(engine->rootEntity() == nullptr);
        delete engine;
    }

    void setRootEntity()
    {
        QAspectEngine *engine = new QAspectEngine;

        QEntity *e = new QEntity;
        e->setObjectName("root");
        engine->setRootEntity(QEntityPtr(e));

        QSharedPointer<QEntity> root = engine->rootEntity();
        QVERIFY(root == e);
        QVERIFY(root->objectName() == "root");
        root = QSharedPointer<QEntity>();
        QVERIFY(engine->rootEntity()->objectName() == "root");

        delete engine;
    }

    void shouldNotCrashInNormalStartupShutdownSequence()
    {
        // GIVEN
        // An initialized aspect engine...
        QAspectEngine engine;
        // ...and a simple aspect
        PrintRootAspect *aspect = new PrintRootAspect;

        // WHEN
        // We register the aspect
        engine.registerAspect(aspect);

        // THEN
        const auto registeredAspects = engine.aspects();
        QCOMPARE(registeredAspects.size(), 1);
        QCOMPARE(registeredAspects.first(), aspect);

        // WHEN
        QEntityPtr entity(new QEntity);
        entity->setObjectName("RootEntity");
        // we set a scene root entity
        engine.setRootEntity(entity);

        QEventLoop eventLoop;
        QTimer::singleShot(100, &eventLoop, SLOT(quit()));
        eventLoop.exec();

        // THEN
        // we don't crash and...
        const auto rootEntity = engine.rootEntity();
        QCOMPARE(rootEntity, entity);

        // WHEN
        // we set an empty/null scene root...
        engine.setRootEntity(QEntityPtr());
        QTimer::singleShot(1000, &eventLoop, SLOT(quit()));

        // ...and allow events to process...
        eventLoop.exec();

        // THEN
        // ... we don't crash.

        // TODO: Add more tests to check for
        // * re-setting a scene
        // * deregistering aspects
        // * destroying the aspect engine
    }

    void shouldNotCrashOnShutdownWhenComponentIsCreatedWithParentBeforeItsEntity()
    {
        // GIVEN
        QEntity *root = new QEntity;
        // A component parented to an entity...
        QComponent *component = new Qt3DCore::QTransform(root);
        // ... created *before* the entity it will be added to.
        QEntity *entity = new QEntity(root);
        entity->addComponent(component);

        // An initialized engine (so that the arbiter has been fed)
        QAspectEngine engine;

        // WHEN
        engine.setRootEntity(QEntityPtr(root));

        // THEN
        // Nothing particular happen on exit, especially no crash
    }

    void shouldRegisterAspectsByName()
    {
        // GIVEN
        QAspectEngine engine;

        // THEN
        QVERIFY(engine.aspects().isEmpty());

        // WHEN
        engine.registerAspect("fake");

        // THEN
        QCOMPARE(engine.aspects().size(), 1);
        QVERIFY(qobject_cast<FakeAspect*>(engine.aspects().first()));
    }

    void shouldListLoadedAspects()
    {
        // GIVEN
        QAspectEngine engine;

        // THEN
        QCOMPARE(engine.executeCommand("list aspects").toString(),
                 QString("No loaded aspect"));

        // WHEN
        engine.registerAspect("fake");

        // THEN
        QCOMPARE(engine.executeCommand("list aspects").toString(),
                 QString("Loaded aspects:\n * fake"));

        // WHEN
        engine.registerAspect("otherfake");

        // THEN
        QCOMPARE(engine.executeCommand("list aspects").toString(),
                 QString("Loaded aspects:\n * fake\n * otherfake"));

        // WHEN
        engine.registerAspect(new FakeAspect3);

        // THEN
        QCOMPARE(engine.executeCommand("list aspects").toString(),
                 QString("Loaded aspects:\n * fake\n * otherfake\n * <unnamed>"));
    }

    void shouldDelegateCommandsToAspects()
    {
        // GIVEN
        QAspectEngine engine;
        engine.registerAspect("fake");
        engine.registerAspect("otherfake");
        engine.registerAspect(new FakeAspect3);

        // WHEN
        QVariant output = engine.executeCommand("fake echo foo bar");

        // THEN
        QVERIFY(output.isValid());
        QCOMPARE(output.toString(), QString("FakeAspect said 'foo bar'"));

        // WHEN
        output = engine.executeCommand("otherfake echo fizz buzz");

        // THEN
        QVERIFY(output.isValid());
        QCOMPARE(output.toString(), QString("FakeAspect2 said 'fizz buzz'"));

        // WHEN
        output = engine.executeCommand("mehfake echo fizz buzz");

        // THEN
        QVERIFY(!output.isValid());

        // WHEN
        output = engine.executeCommand("fake mooh fizz buzz");

        // THEN
        QVERIFY(!output.isValid());
    }
};

QTEST_MAIN(tst_QAspectEngine)

#include "tst_qaspectengine.moc"
