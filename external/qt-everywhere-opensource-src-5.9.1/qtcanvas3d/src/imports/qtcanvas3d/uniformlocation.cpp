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

#include "uniformlocation_p.h"
#include "context3d_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DUniformLocation
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \inherits Canvas3DAbstractObject
 * \brief Contains uniform location id.
 *
 * An uncreatable QML type that contains an uniform location id. You can get it by calling
 * the \l{Context3D::getUniformLocation()}{Context3D.getUniformLocation()} method.
 */

CanvasUniformLocation::CanvasUniformLocation(CanvasGlCommandQueue *queue, QObject *parent) :
    CanvasAbstractObject(queue, parent),
    m_locationId(queue->createResourceId()),
    m_type(-1)
{
}

CanvasUniformLocation::~CanvasUniformLocation()
{
    if (m_locationId)
        queueCommand(CanvasGlCommandQueue::internalClearLocation, m_locationId);
}

GLint CanvasUniformLocation::id()
{
    return m_locationId;
}

GLint CanvasUniformLocation::type()
{
    return m_type;
}

/*!
 * Synchronously retrieves uniform type.
 */
void CanvasUniformLocation::resolveType(GLint programId, CanvasContext *context)
{
    if (m_type < 0) {
        GlSyncCommand syncCommand(CanvasGlCommandQueue::internalGetUniformType, programId);
        syncCommand.data = new QByteArray(name().toLatin1());
        syncCommand.returnValue = &m_type;
        context->scheduleSyncCommand(&syncCommand);
    }
}

QDebug operator<<(QDebug dbg, const CanvasUniformLocation *uLoc)
{
    if (uLoc)
        dbg.nospace() << "Canvas3DUniformLocation("<< (void *) uLoc << ", name:"<< uLoc->name() <<", location:" << uLoc->m_locationId << ")";
    else
        dbg.nospace() << "Canvas3DUniformLocation("<< (void *)(uLoc) << ")";

    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
