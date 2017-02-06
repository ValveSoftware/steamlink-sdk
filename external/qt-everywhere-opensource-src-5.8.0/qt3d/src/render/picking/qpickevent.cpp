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

#include "qpickevent.h"
#include "qpickevent_p.h"
#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QPickEvent
    \inmodule Qt3DRender

    \brief The QPickEvent class holds information when an object is picked
    \sa QPickingSettings, QPickTriangleEvent, QObjectPicker

    \since 5.7
*/

/*!
 * \qmltype PickEvent
 * \instantiates Qt3DRender::QPickEvent
 * \inqmlmodule Qt3D.Render
 * \sa ObjectPicker PickingSettings
 * \brief PickEvent holds information when an object is picked.
 */


/*!
  \fn Qt3DRender::QPickEvent::QPickEvent()
  Constructs a new QPickEvent.
 */
QPickEvent::QPickEvent()
    : QObject(*new QPickEventPrivate())
{
}

/*!
  \fn Qt3DRender::QPickEvent::QPickEvent(const QPointF &position, const QVector3D &intersection, const QVector3D &localIntersection, float distance)
  Constructs a new QPickEvent with the given parameters: \a position, \a intersection, \a localIntersection and \a distance
 */
// NOTE: remove in Qt6
QPickEvent::QPickEvent(const QPointF &position, const QVector3D &worldIntersection, const QVector3D &localIntersection, float distance)
    : QObject(*new QPickEventPrivate())
{
    Q_D(QPickEvent);
    d->m_position = position;
    d->m_distance = distance;
    d->m_worldIntersection = worldIntersection;
    d->m_localIntersection = localIntersection;
}

/*!
  \fn Qt3DRender::QPickEvent::QPickEvent(const QPointF &position, const QVector3D &intersection, const QVector3D &localIntersection, float distance)
  Constructs a new QPickEvent with the given parameters: \a position, \a intersection, \a localIntersection, \a distance, \a button, \a buttons and \a modifiers
 */
QPickEvent::QPickEvent(const QPointF &position, const QVector3D &worldIntersection, const QVector3D &localIntersection, float distance, QPickEvent::Buttons button, int buttons, int modifiers)
    : QObject(*new QPickEventPrivate())
{
    Q_D(QPickEvent);
    d->m_position = position;
    d->m_distance = distance;
    d->m_worldIntersection = worldIntersection;
    d->m_localIntersection = localIntersection;
    d->m_button = button;
    d->m_buttons = buttons;
    d->m_modifiers = modifiers;
}

/*! \internal */
QPickEvent::QPickEvent(QObjectPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{

}

/*! \internal */
QPickEvent::~QPickEvent()
{
}

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::accepted
    Specifies if event has been accepted
*/
/*!
  \property Qt3DRender::QPickEvent::accepted
    Specifies if event has been accepted
 */
/*!
 * \brief QPickEvent::isAccepted
 * \return true if the event has been accepted
 */
bool QPickEvent::isAccepted() const
{
    Q_D(const QPickEvent);
    return d->m_accepted;
}
/*!
 * \brief QPickEvent::setAccepted set if the event has been accepted to \a accepted
 */
void QPickEvent::setAccepted(bool accepted)
{
    Q_D(QPickEvent);
    if (accepted != d->m_accepted) {
        d->m_accepted = accepted;
        emit acceptedChanged(accepted);
    }
}

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::position
    Specifies the position of the event
*/
/*!
  \property Qt3DRender::QPickEvent::position
    Specifies the position of the event
 */
/*!
 * \brief QPickEvent::position
 * \return mouse pointer coordinate of the pick query
 */
QPointF QPickEvent::position() const
{
    Q_D(const QPickEvent);
    return d->m_position;
}

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::distance
    Specifies the distance of the event
*/
/*!
  \property Qt3DRender::QPickEvent::distance
    Specifies the distance of the event
 */
/*!
 * \brief QPickEvent::distance
 * \return distance from camera to pick point
 */
float QPickEvent::distance() const
{
    Q_D(const QPickEvent);
    return d->m_distance;
}

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::worldIntersection
    Specifies the world intersection of the event
*/
/*!
  \property Qt3DRender::QPickEvent::worldIntersection
    Specifies the world intersection of the event
 */
/*!
 * \brief QPickEvent::worldIntersection
 * \return  world coordinate of the pick point
 */
QVector3D QPickEvent::worldIntersection() const
{
    Q_D(const QPickEvent);
    return d->m_worldIntersection;
}

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::localIntersection
    Specifies the world local intersection of the event
*/
/*!
  \property Qt3DRender::QPickEvent::localIntersection
    Specifies the local intersection of the event
 */
/*!
 * \brief QPickEvent::localIntersection
 * \return local coordinate of pick point
 */
QVector3D QPickEvent::localIntersection() const
{
    Q_D(const QPickEvent);
    return d->m_localIntersection;
}

/*!
 * \enum Qt3DRender::QPickEvent::Buttons
 *
 * \value LeftButton
 * \value RightButton
 * \value MiddleButton
 * \value BackButton
 * \value NoButton
 */

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::button
    Specifies mouse button that caused the event
*/
/*!
  \property Qt3DRender::QPickEvent::button
    Specifies mouse button that caused the event
 */
/*!
 * \brief QPickEvent::button
 * \return mouse button that caused the event
 */
QPickEvent::Buttons QPickEvent::button() const
{
    Q_D(const QPickEvent);
    return d->m_button;
}

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::buttons
    Specifies state of the mouse buttons for the event
*/
/*!
  \property Qt3DRender::QPickEvent::buttons
    Specifies state of the mouse buttons for the event
 */
/*!
 * \brief QPickEvent::buttons
 * \return bitfield to be used to check for mouse buttons that may be accompanying the pick event.
 */
int QPickEvent::buttons() const
{
    Q_D(const QPickEvent);
    return d->m_buttons;
}

/*!
 * \enum Qt3DRender::QPickEvent::Modifiers
 *
 * \value NoModifier
 * \value ShiftModifier
 * \value ControlModifier
 * \value AltModifier
 * \value MetaModifier
 * \value KeypadModifier
 */

/*!
    \qmlproperty bool Qt3D.Render::PickEvent::modifiers
    Specifies state of the mouse buttons for the event
*/
/*!
  \property Qt3DRender::QPickEvent::modifiers
    Specifies state of the mouse buttons for the event
 */
/*!
 * \brief QPickEvent::modifiers
 * \return bitfield to be used to check for keyboard modifiers that may be accompanying the pick event.
 */
int QPickEvent::modifiers() const
{
    Q_D(const QPickEvent);
    return d->m_modifiers;
}

} // Qt3DRender

QT_END_NAMESPACE

