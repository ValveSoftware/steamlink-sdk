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

#include <qcamerazoomcontrol.h>
#include  "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraZoomControl


    \brief The QCameraZoomControl class supplies control for
    optical and digital camera zoom.

    \inmodule QtMultimedia


    \ingroup multimedia_control

    The interface name of QCameraZoomControl is \c org.qt-project.qt.camerazoomcontrol/5.0 as
    defined in QCameraZoomControl_iid.


    \sa QMediaService::requestControl(), QCamera
*/

/*!
    \macro QCameraZoomControl_iid

    \c org.qt-project.qt.camerazoomcontrol/5.0

    Defines the interface name of the QCameraZoomControl class.

    \relates QCameraZoomControl
*/

/*!
    Constructs a camera zoom control object with \a parent.
*/

QCameraZoomControl::QCameraZoomControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destruct the camera zoom control object.
*/

QCameraZoomControl::~QCameraZoomControl()
{
}

/*!
  \fn qreal QCameraZoomControl::maximumOpticalZoom() const

  Returns the maximum optical zoom value, or 1.0 if optical zoom is not supported.
*/


/*!
  \fn qreal QCameraZoomControl::maximumDigitalZoom() const

  Returns the maximum digital zoom value, or 1.0 if digital zoom is not supported.
*/


/*!
  \fn qreal QCameraZoomControl::requestedOpticalZoom() const

  Return the requested optical zoom value.
*/

/*!
  \fn qreal QCameraZoomControl::requestedDigitalZoom() const

  Return the requested digital zoom value.
*/

/*!
  \fn qreal QCameraZoomControl::currentOpticalZoom() const

  Return the current optical zoom value.
*/

/*!
  \fn qreal QCameraZoomControl::currentDigitalZoom() const

  Return the current digital zoom value.
*/

/*!
  \fn void QCameraZoomControl::zoomTo(qreal optical, qreal digital)

  Sets \a optical and \a digital zoom values.

  Zooming can be asynchronous with value changes reported with
  currentDigitalZoomChanged() and currentOpticalZoomChanged() signals.

  The backend should expect and correctly handle frequent zoomTo() calls
  during zoom animations or slider movements.
*/


/*!
    \fn void QCameraZoomControl::currentOpticalZoomChanged(qreal zoom)

    Signal emitted when the current optical \a zoom value changed.
*/

/*!
    \fn void QCameraZoomControl::currentDigitalZoomChanged(qreal zoom)

    Signal emitted when the current digital \a zoom value changed.
*/

/*!
    \fn void QCameraZoomControl::requestedOpticalZoomChanged(qreal zoom)

    Signal emitted when the requested optical \a zoom value changed.
*/

/*!
    \fn void QCameraZoomControl::requestedDigitalZoomChanged(qreal zoom)

    Signal emitted when the requested digital \a zoom value changed.
*/


/*!
    \fn void QCameraZoomControl::maximumOpticalZoomChanged(qreal zoom)

    Signal emitted when the maximum supported optical \a zoom value changed.

    The maximum supported zoom value can depend on other camera settings,
    like focusing mode.
*/

/*!
    \fn void QCameraZoomControl::maximumDigitalZoomChanged(qreal zoom)

    Signal emitted when the maximum supported digital \a zoom value changed.

    The maximum supported zoom value can depend on other camera settings,
    like capture mode or resolution.
*/

#include "moc_qcamerazoomcontrol.cpp"
QT_END_NAMESPACE

