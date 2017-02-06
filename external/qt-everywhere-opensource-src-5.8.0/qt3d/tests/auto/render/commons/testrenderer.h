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

#ifndef TESTRENDERER_H
#define TESTRENDERER_H

#include <Qt3DRender/private/abstractrenderer_p.h>

QT_BEGIN_NAMESPACE

class TestRenderer : public Qt3DRender::Render::AbstractRenderer
{
public:
    TestRenderer();
    ~TestRenderer();

    void dumpInfo() const Q_DECL_OVERRIDE {}
    API api() const Q_DECL_OVERRIDE { return AbstractRenderer::OpenGL; }
    qint64 time() const Q_DECL_OVERRIDE { return 0; }
    void setTime(qint64 time) Q_DECL_OVERRIDE { Q_UNUSED(time); }
    void setNodeManagers(Qt3DRender::Render::NodeManagers *managers) Q_DECL_OVERRIDE { Q_UNUSED(managers); }
    void setServices(Qt3DCore::QServiceLocator *services) Q_DECL_OVERRIDE { Q_UNUSED(services); }
    void setSurfaceExposed(bool exposed) Q_DECL_OVERRIDE { Q_UNUSED(exposed); }
    Qt3DRender::Render::NodeManagers *nodeManagers() const Q_DECL_OVERRIDE { return nullptr; }
    Qt3DCore::QServiceLocator *services() const Q_DECL_OVERRIDE { return nullptr; }
    void initialize() Q_DECL_OVERRIDE {}
    void shutdown() Q_DECL_OVERRIDE {}
    void releaseGraphicsResources() Q_DECL_OVERRIDE {}
    void render() Q_DECL_OVERRIDE {}
    void doRender() Q_DECL_OVERRIDE {}
    void cleanGraphicsResources() Q_DECL_OVERRIDE {}
    bool isRunning() const Q_DECL_OVERRIDE { return true; }
    bool shouldRender() Q_DECL_OVERRIDE { return true; }
    void skipNextFrame() Q_DECL_OVERRIDE {}
    QVector<Qt3DCore::QAspectJobPtr> renderBinJobs() Q_DECL_OVERRIDE { return QVector<Qt3DCore::QAspectJobPtr>(); }
    Qt3DCore::QAspectJobPtr pickBoundingVolumeJob() Q_DECL_OVERRIDE { return Qt3DCore::QAspectJobPtr(); }
    Qt3DCore::QAspectJobPtr syncTextureLoadingJob() Q_DECL_OVERRIDE { return Qt3DCore::QAspectJobPtr(); }
    void setSceneRoot(Qt3DCore::QBackendNodeFactory *factory, Qt3DRender::Render::Entity *root) Q_DECL_OVERRIDE { Q_UNUSED(factory);  Q_UNUSED(root); }
    Qt3DRender::Render::Entity *sceneRoot() const Q_DECL_OVERRIDE { return nullptr; }
    Qt3DRender::Render::FrameGraphNode *frameGraphRoot() const Q_DECL_OVERRIDE { return nullptr; }
    Qt3DCore::QAbstractFrameAdvanceService *frameAdvanceService() const Q_DECL_OVERRIDE { return nullptr; }
    void registerEventFilter(Qt3DCore::QEventFilterService *service) Q_DECL_OVERRIDE { Q_UNUSED(service); }
    void setSettings(Qt3DRender::Render::RenderSettings *settings) Q_DECL_OVERRIDE { Q_UNUSED(settings); }
    Qt3DRender::Render::RenderSettings *settings() const Q_DECL_OVERRIDE { return nullptr; }

    void markDirty(Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet changes, Qt3DRender::Render::BackendNode *node) Q_DECL_OVERRIDE;
    Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet dirtyBits() Q_DECL_OVERRIDE;
    void clearDirtyBits(Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet changes) Q_DECL_OVERRIDE;

    void resetDirty();
    QVariant executeCommand(const QStringList &args) Q_DECL_OVERRIDE;

    void setOffscreenSurfaceHelper(Qt3DRender::Render::OffscreenSurfaceHelper *helper) Q_DECL_OVERRIDE;
    QSurfaceFormat format() Q_DECL_OVERRIDE;

protected:
    Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet m_changes;
};

QT_END_NAMESPACE

#endif // TESTRENDERER_H
