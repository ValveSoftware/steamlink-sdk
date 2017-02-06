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

#include "qpicktriangleevent.h"
#include "qpickevent_p.h"
#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class QPickTriangleEventPrivate : public QPickEventPrivate
{
public:
    QPickTriangleEventPrivate()
        : QPickEventPrivate()
        , m_triangleIndex(0)
        , m_vertex1Index(0)
        , m_vertex2Index(0)
        , m_vertex3Index(0)
    {
    }

    uint m_triangleIndex;
    uint m_vertex1Index;
    uint m_vertex2Index;
    uint m_vertex3Index;
};

/*!
    \class Qt3DRender::QPickTriangleEvent
    \inmodule Qt3DRender

    \brief The QPickTriangleEvent class holds information when a triangle is picked

    \sa QPickEvent
    \since 5.7
*/

/*!
 * \qmltype PickTriangleEvent
 * \instantiates Qt3DRender::QPickTriangleEvent
 * \inqmlmodule Qt3D.Render
 * \brief PickTriangleEvent holds information when a triangle is picked.
 * \sa ObjectPicker
 */


/*!
  \fn Qt3DRender::QPickTriangleEvent::QPickTriangleEvent()
  Constructs a new QPickEvent.
 */
QPickTriangleEvent::QPickTriangleEvent()
    : QPickEvent(*new QPickTriangleEventPrivate())
{
}

/*!
 * \brief QPickTriangleEvent::QPickTriangleEvent Constructs a new QPickEvent with the given parameters
 * \a position,
 * \a intersection,
 * \a localIntersection,
 * \a distance,
 * \a triangleIndex,
 * \a vertex1Index,
 * \a vertex2Index and
 * \a vertex3Index
 */
// NOTE: remove in Qt6
QPickTriangleEvent::QPickTriangleEvent(const QPointF &position, const QVector3D &worldIntersection, const QVector3D &localIntersection, float distance,
                                       uint triangleIndex, uint vertex1Index, uint vertex2Index, uint vertex3Index)
    : QPickEvent(*new QPickTriangleEventPrivate())
{
    Q_D(QPickTriangleEvent);
    d->m_position = position;
    d->m_distance = distance;
    d->m_worldIntersection = worldIntersection;
    d->m_localIntersection = localIntersection;
    d->m_triangleIndex = triangleIndex;
    d->m_vertex1Index = vertex1Index;
    d->m_vertex2Index = vertex2Index;
    d->m_vertex3Index = vertex3Index;
}

QPickTriangleEvent::QPickTriangleEvent(const QPointF &position, const QVector3D &worldIntersection, const QVector3D &localIntersection, float distance, uint triangleIndex, uint vertex1Index, uint vertex2Index, uint vertex3Index, QPickEvent::Buttons button, int buttons, int modifiers)
    : QPickEvent(*new QPickTriangleEventPrivate())
{
    Q_D(QPickTriangleEvent);
    d->m_position = position;
    d->m_distance = distance;
    d->m_worldIntersection = worldIntersection;
    d->m_localIntersection = localIntersection;
    d->m_triangleIndex = triangleIndex;
    d->m_vertex1Index = vertex1Index;
    d->m_vertex2Index = vertex2Index;
    d->m_vertex3Index = vertex3Index;
    d->m_button = button;
    d->m_buttons = buttons;
    d->m_modifiers = modifiers;
}

/*! \internal */
QPickTriangleEvent::~QPickTriangleEvent()
{
}

/*!
    \qmlproperty uint Qt3D.Render::PickTriangleEvent::triangleIndex
    Specifies the triangle index of the event
*/
/*!
  \property Qt3DRender::QPickTriangleEvent::triangleIndex
    Specifies the triangle index of the event
 */
/*!
 * \brief QPickTriangleEvent::triangleIndex
 * Returns the index of the picked triangle
 */
uint QPickTriangleEvent::triangleIndex() const
{
    Q_D(const QPickTriangleEvent);
    return d->m_triangleIndex;
}

/*!
    \qmlproperty uint Qt3D.Render::PickTriangleEvent::vertex1Index
    Specifies the vertex 1 index of the event
*/
/*!
  \property Qt3DRender::QPickTriangleEvent::vertex1Index
    Specifies the vertex 1 index of the event
 */
/*!
 * \brief QPickTriangleEvent::vertex1Index
 * Returns the index of the first point of the picked triangle
 */
uint QPickTriangleEvent::vertex1Index() const
{
    Q_D(const QPickTriangleEvent);
    return d->m_vertex1Index;
}

/*!
    \qmlproperty uint Qt3D.Render::PickTriangleEvent::vertex2Index
    Specifies the vertex 2 index of the event
*/
/*!
  \property Qt3DRender::QPickTriangleEvent::vertex2Index
    Specifies the vertex 2 index of the event
 */
/*!
 * \brief QPickTriangleEvent::vertex2Index
 * Returns the index of the second point of the picked triangle
 */
uint QPickTriangleEvent::vertex2Index() const
{
    Q_D(const QPickTriangleEvent);
    return d->m_vertex2Index;
}

/*!
    \qmlproperty uint Qt3D.Render::PickTriangleEvent::vertex3Index
    Specifies the vertex 3 index of the event
*/
/*!
  \property Qt3DRender::QPickTriangleEvent::vertex3Index
    Specifies the vertex 3 index of the event
 */
/*!
 * \brief QPickTriangleEvent::vertex3Index
 * \returns index of third point of picked triangle
 */
uint QPickTriangleEvent::vertex3Index() const
{
    Q_D(const QPickTriangleEvent);
    return d->m_vertex3Index;
}

} // Qt3DRender

QT_END_NAMESPACE

