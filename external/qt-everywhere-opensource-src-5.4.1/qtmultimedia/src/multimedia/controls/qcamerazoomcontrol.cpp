/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

