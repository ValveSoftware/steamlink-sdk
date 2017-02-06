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

#include "loadscenejob_p.h"
#include <private/renderer_p.h>
#include <private/nodemanagers_p.h>
#include <private/scenemanager_p.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/qsceneiohandler_p.h>
#include <Qt3DRender/qsceneloader.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

LoadSceneJob::LoadSceneJob(const QUrl &source, Qt3DCore::QNodeId m_sceneComponent)
    : QAspectJob()
    , m_source(source)
    , m_sceneComponent(m_sceneComponent)
    , m_managers(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::LoadScene, 0);
}

NodeManagers *LoadSceneJob::nodeManagers() const
{
    return m_managers;
}

QList<QSceneIOHandler *> LoadSceneJob::sceneIOHandlers() const
{
    return m_sceneIOHandlers;
}

QUrl LoadSceneJob::source() const
{
    return m_source;
}

Qt3DCore::QNodeId LoadSceneJob::sceneComponentId() const
{
    return m_sceneComponent;
}

void LoadSceneJob::run()
{
    // Iterate scene IO handlers until we find one that can handle this file type
    Qt3DCore::QEntity *sceneSubTree = nullptr;

    Scene *scene = m_managers->sceneManager()->lookupResource(m_sceneComponent);
    Q_ASSERT(scene);

    // Reset status
    scene->setStatus(QSceneLoader::None);
    QSceneLoader::Status finalStatus = QSceneLoader::None;

    // Perform the loading only if the source wasn't explicitly set to empty
    if (!m_source.isEmpty()) {
        finalStatus = QSceneLoader::Error;
        for (QSceneIOHandler *sceneIOHandler : qAsConst(m_sceneIOHandlers)) {
            if (!sceneIOHandler->isFileTypeSupported(m_source))
                continue;

            // If the file type is supported -> enter Loading status
            scene->setStatus(QSceneLoader::Loading);

            // File type is supported, try to load it
            sceneIOHandler->setSource(m_source);
            sceneSubTree = sceneIOHandler->scene();
            if (sceneSubTree != nullptr) {
                // Successfully built a subtree
                finalStatus = QSceneLoader::Ready;
                break;
            }
        }
    }

    // If the sceneSubTree is null it will trigger the frontend to unload
    // any subtree it may hold
    // Set clone of sceneTree in sceneComponent. This will move the sceneSubTree
    // to the QCoreApplication thread which is where the frontend object tree lives.
    scene->setSceneSubtree(sceneSubTree);

    // Note: the status is set after the subtree so that bindinds depending on the status
    // in the frontend will be consistent
    scene->setStatus(finalStatus);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
