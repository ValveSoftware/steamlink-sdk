/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include <qcamerafocuscontrol.h>
#include  "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraFocusControl


    \brief The QCameraFocusControl class supplies control for
    focusing related camera parameters.

    \inmodule QtMultimedia


    \ingroup multimedia_control

    The interface name of QCameraFocusControl is \c org.qt-project.qt.camerafocuscontrol/5.0 as
    defined in QCameraFocusControl_iid.


    \sa QMediaService::requestControl(), QCamera
*/

/*!
    \macro QCameraFocusControl_iid

    \c org.qt-project.qt.camerafocuscontrol/5.0

    Defines the interface name of the QCameraFocusControl class.

    \relates QCameraFocusControl
*/

/*!
    Constructs a camera control object with \a parent.
*/

QCameraFocusControl::QCameraFocusControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destruct the camera control object.
*/

QCameraFocusControl::~QCameraFocusControl()
{
}


/*!
  \fn QCameraFocus::FocusModes QCameraFocusControl::focusMode() const

  Returns the focus mode being used.
*/


/*!
  \fn void QCameraFocusControl::setFocusMode(QCameraFocus::FocusModes mode)

  Set the focus mode to \a mode.
*/


/*!
  \fn bool QCameraFocusControl::isFocusModeSupported(QCameraFocus::FocusModes mode) const

  Returns true if focus \a mode is supported.
*/

/*!
  \fn QCameraFocusControl::focusPointMode() const

  Returns the camera focus point selection mode.
*/

/*!
  \fn QCameraFocusControl::setFocusPointMode(QCameraFocus::FocusPointMode mode)

  Sets the camera focus point selection \a mode.
*/

/*!
  \fn QCameraFocusControl::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const

  Returns true if the camera focus point \a mode is supported.
*/

/*!
  \fn QCameraFocusControl::customFocusPoint() const

  Return the position of custom focus point, in relative frame coordinates:
  QPointF(0,0) points to the left top frame point, QPointF(0.5,0.5) points to the frame center.

  Custom focus point is used only in FocusPointCustom focus mode.
*/

/*!
  \fn QCameraFocusControl::setCustomFocusPoint(const QPointF &point)

  Sets the custom focus \a point.

  If camera supports fixed set of focus points,
  it should use the nearest supported focus point,
  and return the actual focus point with QCameraFocusControl::focusZones().

  \sa QCameraFocusControl::customFocusPoint(), QCameraFocusControl::focusZones()
*/

/*!
  \fn QCameraFocusControl::focusZones() const

  Returns the list of zones, the camera is using for focusing or focused on.
*/

/*!
  \fn QCameraFocusControl::focusZonesChanged()

  Signal is emitted when the set of zones, camera focused on is changed.

  Usually the zones list is changed when the camera is focused.

  \sa QCameraFocusControl::focusZones()
*/

/*!
  \fn void QCameraFocusControl::focusModeChanged(QCameraFocus::FocusModes mode)

  Signal is emitted when the focus \a mode is changed,
  usually in result of QCameraFocusControl::setFocusMode call or capture mode changes.

  \sa QCameraFocusControl::focusMode(), QCameraFocusControl::setFocusMode()
*/

/*!
  \fn void QCameraFocusControl::focusPointModeChanged(QCameraFocus::FocusPointMode mode)

  Signal is emitted when the focus point \a mode is changed,
  usually in result of QCameraFocusControl::setFocusPointMode call or capture mode changes.

  \sa QCameraFocusControl::focusPointMode(), QCameraFocusControl::setFocusPointMode()
*/

/*!
  \fn void QCameraFocusControl::customFocusPointChanged(const QPointF &point)

  Signal is emitted when the custom focus \a point is changed.

  \sa QCameraFocusControl::customFocusPoint(), QCameraFocusControl::setCustomFocusPoint()
*/



#include "moc_qcamerafocuscontrol.cpp"
QT_END_NAMESPACE

