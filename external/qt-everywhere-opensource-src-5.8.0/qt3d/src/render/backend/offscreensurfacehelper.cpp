/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "offscreensurfacehelper_p.h"

#include <Qt3DRender/private/abstractrenderer_p.h>
#include <QtGui/qoffscreensurface.h>
#include <QtGui/qsurfaceformat.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

/*! \internal */
OffscreenSurfaceHelper::OffscreenSurfaceHelper(AbstractRenderer *renderer,
                                               QObject *parent)
    : QObject(parent)
    , m_renderer(renderer)
    , m_offscreenSurface(nullptr)
{
    Q_ASSERT(renderer);
}

/*!
 * \internal
 * Called in context of main thread to create an offscreen surface
 * which can later be made current with the Qt 3D OpenGL context to
 * then allow graphics resources to be released cleanly.
 */
void OffscreenSurfaceHelper::createOffscreenSurface()
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    m_offscreenSurface = new QOffscreenSurface;
    m_offscreenSurface->setParent(this);
    m_offscreenSurface->setFormat(m_renderer->format());
    m_offscreenSurface->create();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
