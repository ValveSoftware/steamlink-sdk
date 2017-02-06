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

#include "abstractobject3d_p.h"
#include "glcommandqueue_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DAbstractObject
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Base type for Canvas3D types representing OpenGL resources.
 *
 * An uncreatable QML type that is the base type for other Canvas3D types that represent
 * OpenGL resources.
 */

CanvasAbstractObject::CanvasAbstractObject(CanvasGlCommandQueue *queue, QObject *parent) :
    QObject(parent),
    m_hasName(false),
    m_invalidated(false),
    m_commandQueue(queue)

{
    m_name = QString("0x%1").arg((long long) this, 0, 16);
}

CanvasAbstractObject::~CanvasAbstractObject()
{
}

/*!
 * \qmlproperty string Canvas3DAbstractObject::name
 * Name of the object.
 */
void CanvasAbstractObject::setName(const QString &name)
{
    if (m_name == name)
        return;

    m_name = name;
    m_hasName = true;

    emit nameChanged(m_name);
}

const QString &CanvasAbstractObject::name() const
{
    return m_name;
}

bool CanvasAbstractObject::hasSpecificName() const
{
    return m_hasName;
}

/*!
 * \qmlproperty bool Canvas3DAbstractObject::invalidated
 * Indicates if this object has been invalidated. Invalidated objects cannot be valid parameters
 * in Context3D methods and will result in \c{Context3D.INVALID_OPERATION} error if used.
 * Objects are invalidated when context is lost and cannot be validated again.
 *
 * \sa {Canvas3D::contextLost}{Canvas3D.contextLost}
 */
bool CanvasAbstractObject::invalidated() const
{
    return m_invalidated;
}

void CanvasAbstractObject::setInvalidated(bool invalidated)
{
    m_invalidated = invalidated;
}

void CanvasAbstractObject::queueCommand(CanvasGlCommandQueue::GlCommandId id, GLint p1, GLint p2)
{
    if (!m_invalidated)
        m_commandQueue->queueCommand(id, p1, p2);
}

void CanvasAbstractObject::queueCommand(CanvasGlCommandQueue::GlCommandId id, QByteArray *data,
                                        GLint p1, GLint p2)
{
    if (!m_invalidated) {
        GlCommand &command = m_commandQueue->queueCommand(id, p1, p2);
        command.data = data;
    } else {
        // We need to delete the data since we are not passing it to a command
        delete data;
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
