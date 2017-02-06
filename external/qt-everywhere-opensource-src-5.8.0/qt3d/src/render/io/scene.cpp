/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "scene_p.h"
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DRender/qsceneloader.h>
#include <Qt3DRender/private/qsceneloader_p.h>
#include <Qt3DRender/private/scenemanager_p.h>
#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Scene::Scene()
    : BackendNode(QBackendNode::ReadWrite)
    , m_sceneManager(nullptr)
{
}

void Scene::cleanup()
{
    m_source.clear();
}

void Scene::setStatus(QSceneLoader::Status status)
{
    // Send the new subtree to the frontend or notify failure
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("status");
    e->setValue(QVariant::fromValue(status));
    notifyObservers(e);
}

void Scene::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QSceneLoaderData>>(change);
    const auto &data = typedChange->data;
    m_source = data.source;
    Q_ASSERT(m_sceneManager);
    m_sceneManager->addSceneData(m_source, peerId());
}

void Scene::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("source")) {
            m_source = propertyChange->value().toUrl();
            m_sceneManager->addSceneData(m_source, peerId());
        }
    }
    markDirty(AbstractRenderer::AllDirty);
}

QUrl Scene::source() const
{
    return m_source;
}

void Scene::setSceneSubtree(Qt3DCore::QEntity *subTree)
{
    if (subTree) {
        // Move scene sub tree to the application thread so that it can be grafted in.
        const auto appThread = QCoreApplication::instance()->thread();
        subTree->moveToThread(appThread);
    }

    // Send the new subtree to the frontend or notify failure
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("scene");
    e->setValue(QVariant::fromValue(subTree));
    notifyObservers(e);
}

void Scene::setSceneManager(SceneManager *manager)
{
    if (m_sceneManager != manager)
        m_sceneManager = manager;
}

RenderSceneFunctor::RenderSceneFunctor(AbstractRenderer *renderer, SceneManager *sceneManager)
    : m_sceneManager(sceneManager)
    , m_renderer(renderer)
{
}

Qt3DCore::QBackendNode *RenderSceneFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    Scene *scene = m_sceneManager->getOrCreateResource(change->subjectId());
    scene->setSceneManager(m_sceneManager);
    scene->setRenderer(m_renderer);
    return scene;
}

Qt3DCore::QBackendNode *RenderSceneFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_sceneManager->lookupResource(id);
}

void RenderSceneFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_sceneManager->releaseResource(id);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
