/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "texture3d_p.h"
#include "glcommandqueue_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DTexture
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \inherits Canvas3DAbstractObject
 * \brief Contains an OpenGL texture.
 *
 * An uncreatable QML type that contains an OpenGL texture. You can get it by calling
 * the \l{Context3D::createTexture()}{Context3D.createTexture()} method.
 */

CanvasTexture::CanvasTexture(CanvasGlCommandQueue *queue, CanvasContext *context,
                             QQuickItem *quickItem) :
    CanvasAbstractObject(queue, context),
    m_textureId(queue->createResourceId()),
    m_isAlive(true),
    m_context(context),
    m_quickItem(quickItem)
{
    if (m_quickItem)
        connect(m_quickItem, &QObject::destroyed, this, &CanvasTexture::handleItemDestroyed);
    else
        queueCommand(CanvasGlCommandQueue::glGenTextures, m_textureId);
}

CanvasTexture::~CanvasTexture()
{
    del();
}

void CanvasTexture::bind(CanvasContext::glEnums target)
{
    if (m_textureId)
        queueCommand(CanvasGlCommandQueue::glBindTexture, GLint(target), m_textureId);
}

GLint CanvasTexture::textureId() const
{
    if (!m_isAlive)
        return 0;

    return m_textureId;
}

void CanvasTexture::handleItemDestroyed()
{
    del();
}

bool CanvasTexture::isAlive() const
{
    return bool(m_textureId);
}

void CanvasTexture::del()
{
    if (!invalidated() && m_textureId) {
        if (m_quickItem) {
            m_context->quickItemToTextureMap().remove(m_quickItem);
            m_quickItem = 0;
            queueCommand(CanvasGlCommandQueue::internalClearQuickItemAsTexture, m_textureId);
        } else {
            queueCommand(CanvasGlCommandQueue::glDeleteTextures, m_textureId);
        }
    }
    m_textureId = 0;
}

QDebug operator<<(QDebug dbg, const CanvasTexture *texture)
{
    if (texture)
        dbg.nospace() << "Canvas3DTexture("<< ((void*) texture) << ", name:" << texture->name() << ", id:" << texture->textureId() << ")";
    else
        dbg.nospace() << "Canvas3DTexture("<< ((void*) texture) <<")";
    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
