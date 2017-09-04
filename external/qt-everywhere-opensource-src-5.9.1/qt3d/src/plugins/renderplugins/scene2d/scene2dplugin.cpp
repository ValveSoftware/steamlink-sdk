/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "scene2dplugin.h"

#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DQuickScene2D/qscene2d.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

template <typename Backend>
class Scene2DBackendNodeMapper : public Qt3DCore::QBackendNodeMapper
{
public:
    explicit Scene2DBackendNodeMapper(Render::AbstractRenderer *renderer,
                                      Render::Scene2DNodeManager *manager)
        : m_manager(manager)
        , m_renderer(renderer)
    {
    }

    Qt3DCore::QBackendNode *create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const Q_DECL_FINAL
    {
        Backend *backend = m_manager->getOrCreateResource(change->subjectId());
        backend->setRenderer(m_renderer);
        return backend;
    }

    Qt3DCore::QBackendNode *get(Qt3DCore::QNodeId id) const Q_DECL_FINAL
    {
        return m_manager->lookupResource(id);
    }

    void destroy(Qt3DCore::QNodeId id) const Q_DECL_FINAL
    {
        m_manager->releaseResource(id);
    }

private:
    Render::Scene2DNodeManager *m_manager;
    Render::AbstractRenderer *m_renderer;
};

Scene2DPlugin::Scene2DPlugin()
    : m_scene2dNodeManager(new Render::Scene2DNodeManager())
{

}

Scene2DPlugin::~Scene2DPlugin()
{
    delete m_scene2dNodeManager;
}

bool Scene2DPlugin::registerBackendTypes(QRenderAspect *aspect,
                                         AbstractRenderer *renderer)
{
    registerBackendType(aspect, Qt3DRender::Quick::QScene2D::staticMetaObject,
                QSharedPointer<Scene2DBackendNodeMapper<Render::Quick::Scene2D> >
                    ::create(renderer, m_scene2dNodeManager));
    return true;
}
bool Scene2DPlugin::unregisterBackendTypes(QRenderAspect *aspect)
{
    unregisterBackendType(aspect, Qt3DRender::Quick::QScene2D::staticMetaObject);
    return true;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
