/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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

#include <QtTest/QTest>
#include <Qt3DRender/private/scene_p.h>
#include <Qt3DRender/private/loadscenejob_p.h>
#include <Qt3DRender/private/qsceneiohandler_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/scenemanager_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"

class TestIOSceneHandler : public Qt3DRender::QSceneIOHandler
{
public:
    explicit TestIOSceneHandler(bool supportsFormat, bool shouldFail)
        : m_supportsFormat(supportsFormat)
        , m_shouldFail(shouldFail)
    {}

    void setSource(const QUrl &source) Q_DECL_OVERRIDE
    {
        m_source = source;
    }

    bool isFileTypeSupported(const QUrl &) const Q_DECL_OVERRIDE
    {
        return m_supportsFormat;
    }

    Qt3DCore::QEntity *scene(const QString &) Q_DECL_OVERRIDE
    {
        return m_shouldFail ? nullptr : new Qt3DCore::QEntity();
    }

    Qt3DCore::QEntity *node(const QString &) Q_DECL_OVERRIDE
    {
        return m_shouldFail ? nullptr : new Qt3DCore::QEntity();
    }

    QUrl source() const
    {
        return m_source;
    }

private:
    QUrl m_source;
    bool m_supportsFormat;
    bool m_shouldFail;
};

class tst_LoadSceneJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        QUrl url;
        Qt3DCore::QNodeId nodeId;
        Qt3DRender::Render::LoadSceneJob backendLoadSceneJob(url, nodeId);

        // THEN
        QCOMPARE(backendLoadSceneJob.source(), url);
        QCOMPARE(backendLoadSceneJob.sceneComponentId(), nodeId);
        QVERIFY(backendLoadSceneJob.nodeManagers() == nullptr);
        QCOMPARE(backendLoadSceneJob.sceneIOHandlers().size(), 0);
    }

    void checkInitialize()
    {
        // GIVEN
        const QUrl url(QStringLiteral("URL"));
        const Qt3DCore::QNodeId sceneId = Qt3DCore::QNodeId::createId();
        Qt3DRender::Render::NodeManagers nodeManagers;
        TestIOSceneHandler fakeHandler(true, true);

        // WHEN
        Qt3DRender::Render::LoadSceneJob backendLoadSceneJob(url, sceneId);
        backendLoadSceneJob.setNodeManagers(&nodeManagers);
        backendLoadSceneJob.setSceneIOHandlers(QList<Qt3DRender::QSceneIOHandler *>() << &fakeHandler);

        // THEN
        QCOMPARE(backendLoadSceneJob.source(), url);
        QCOMPARE(backendLoadSceneJob.sceneComponentId(), sceneId);
        QVERIFY(backendLoadSceneJob.nodeManagers() == &nodeManagers);
        QCOMPARE(backendLoadSceneJob.sceneIOHandlers().size(), 1);
        QCOMPARE(backendLoadSceneJob.sceneIOHandlers().first(), &fakeHandler);
    }

    void checkRunValidSourceSupportedFormat()
    {
        // GIVEN
        const QUrl url(QStringLiteral("URL"));
        TestArbiter arbiter;
        Qt3DRender::Render::NodeManagers nodeManagers;
        TestIOSceneHandler fakeHandler(true, false);
        Qt3DCore::QNodeId sceneId = Qt3DCore::QNodeId::createId();
        Qt3DRender::Render::Scene *scene = nodeManagers.sceneManager()->getOrCreateResource(sceneId);

        // THEN
        QVERIFY(scene != nullptr);
        Qt3DCore::QBackendNodePrivate::get(scene)->setArbiter(&arbiter);

        // WHEN
        Qt3DRender::Render::LoadSceneJob loadSceneJob(url, sceneId);
        loadSceneJob.setNodeManagers(&nodeManagers);
        loadSceneJob.setSceneIOHandlers(QList<Qt3DRender::QSceneIOHandler *>() << &fakeHandler);
        loadSceneJob.run();

        // THEN
        QCOMPARE(arbiter.events.count(), 4);
        auto change = arbiter.events.at(0).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::None);

        change = arbiter.events.at(1).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::Loading);

        change = arbiter.events.at(2).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "scene");
        QVERIFY(change->value().value<Qt3DCore::QEntity *>() != nullptr);
        delete change->value().value<Qt3DCore::QEntity *>();

        change = arbiter.events.at(3).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::Ready);
    }

    void checkEmptySource()
    {
        // GIVEN
        QUrl url;
        TestArbiter arbiter;
        Qt3DRender::Render::NodeManagers nodeManagers;
        TestIOSceneHandler fakeHandler(true, false);
        Qt3DCore::QNodeId sceneId = Qt3DCore::QNodeId::createId();
        Qt3DRender::Render::Scene *scene = nodeManagers.sceneManager()->getOrCreateResource(sceneId);

        // THEN
        QVERIFY(scene != nullptr);
        Qt3DCore::QBackendNodePrivate::get(scene)->setArbiter(&arbiter);

        // WHEN
        Qt3DRender::Render::LoadSceneJob loadSceneJob(url, sceneId);
        loadSceneJob.setNodeManagers(&nodeManagers);
        loadSceneJob.setSceneIOHandlers(QList<Qt3DRender::QSceneIOHandler *>() << &fakeHandler);
        loadSceneJob.run();

        // THEN
        QCOMPARE(arbiter.events.count(), 3);
        auto change = arbiter.events.at(0).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::None);

        change = arbiter.events.at(1).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "scene");
        QVERIFY(change->value().value<Qt3DCore::QEntity *>() == nullptr);

        change = arbiter.events.at(2).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::None);
    }

    void checkRunValidSourceUnsupportedFormat()
    {
        // GIVEN
        const QUrl url(QStringLiteral("URL"));
        TestArbiter arbiter;
        Qt3DRender::Render::NodeManagers nodeManagers;
        TestIOSceneHandler fakeHandler(false, false);
        Qt3DCore::QNodeId sceneId = Qt3DCore::QNodeId::createId();
        Qt3DRender::Render::Scene *scene = nodeManagers.sceneManager()->getOrCreateResource(sceneId);

        // THEN
        QVERIFY(scene != nullptr);
        Qt3DCore::QBackendNodePrivate::get(scene)->setArbiter(&arbiter);

        // WHEN
        Qt3DRender::Render::LoadSceneJob loadSceneJob(url, sceneId);
        loadSceneJob.setNodeManagers(&nodeManagers);
        loadSceneJob.setSceneIOHandlers(QList<Qt3DRender::QSceneIOHandler *>() << &fakeHandler);
        loadSceneJob.run();

        // THEN
        QCOMPARE(arbiter.events.count(), 3);
        auto change = arbiter.events.at(0).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::None);

        change = arbiter.events.at(1).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "scene");
        QVERIFY(change->value().value<Qt3DCore::QEntity *>() == nullptr);

        change = arbiter.events.at(2).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::Error);
    }

    void checkRunErrorAtLoading()
    {
        // GIVEN
        const QUrl url(QStringLiteral("URL"));
        TestArbiter arbiter;
        Qt3DRender::Render::NodeManagers nodeManagers;
        TestIOSceneHandler fakeHandler(true, true);
        Qt3DCore::QNodeId sceneId = Qt3DCore::QNodeId::createId();
        Qt3DRender::Render::Scene *scene = nodeManagers.sceneManager()->getOrCreateResource(sceneId);

        // THEN
        QVERIFY(scene != nullptr);
        Qt3DCore::QBackendNodePrivate::get(scene)->setArbiter(&arbiter);

        // WHEN
        Qt3DRender::Render::LoadSceneJob loadSceneJob(url, sceneId);
        loadSceneJob.setNodeManagers(&nodeManagers);
        loadSceneJob.setSceneIOHandlers(QList<Qt3DRender::QSceneIOHandler *>() << &fakeHandler);
        loadSceneJob.run();

        // THEN
        QCOMPARE(arbiter.events.count(), 4);
        auto change = arbiter.events.at(0).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::None);

        change = arbiter.events.at(1).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::Loading);

        change = arbiter.events.at(2).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "scene");
        QVERIFY(change->value().value<Qt3DCore::QEntity *>() == nullptr);
        delete change->value().value<Qt3DCore::QEntity *>();

        change = arbiter.events.at(3).staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), scene->peerId());
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::Error);
    }
};

QTEST_MAIN(tst_LoadSceneJob)

#include "tst_loadscenejob.moc"
