/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qcollisionqueryresult_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

QCollisionQueryResultPrivate::QCollisionQueryResultPrivate()
    : QSharedData()
{
}

QCollisionQueryResultPrivate::QCollisionQueryResultPrivate(const QCollisionQueryResultPrivate &copy)
    : QSharedData(copy)
    , m_handle(copy.m_handle)
    , m_hits(copy.m_hits)
{
}

void QCollisionQueryResultPrivate::addEntityHit(Qt3DCore::QNodeId entity, const QVector3D& intersection, float distance)
{
    m_hits.append(QCollisionQueryResult::Hit(entity, intersection, distance));
}

void QCollisionQueryResultPrivate::setHandle(const QQueryHandle &handle)
{
    m_handle = handle;
}

QCollisionQueryResult::QCollisionQueryResult()
    : d_ptr(new QCollisionQueryResultPrivate())
{
}

QCollisionQueryResult::QCollisionQueryResult(const QCollisionQueryResult &result)
    : d_ptr(result.d_ptr)
{
}

QCollisionQueryResult::~QCollisionQueryResult()
{
}

QCollisionQueryResult &QCollisionQueryResult::operator=(const QCollisionQueryResult &result)
{
    d_ptr = result.d_ptr;
    return *this;
}

QVector<QCollisionQueryResult::Hit> QCollisionQueryResult::hits() const
{
    Q_D(const QCollisionQueryResult);
    return d->m_hits;
}

QVector<Qt3DCore::QNodeId> QCollisionQueryResult::entitiesHit() const
{
    Q_D(const QCollisionQueryResult);
    QVector<Qt3DCore::QNodeId> result;
    for (const Hit& hit : d->m_hits)
        result << hit.m_entityId;
    return result;
}

/*!
    \internal
*/
QCollisionQueryResult::QCollisionQueryResult(QCollisionQueryResultPrivate &p)
    : d_ptr(&p)
{
}

/*!
    \internal
*/
QCollisionQueryResultPrivate *QCollisionQueryResult::d_func()
{
    return d_ptr.data();
}

QQueryHandle QCollisionQueryResult::handle() const
{
    Q_D(const QCollisionQueryResult);
    return d->m_handle;
}

} // Qt3DRender

QT_END_NAMESPACE
