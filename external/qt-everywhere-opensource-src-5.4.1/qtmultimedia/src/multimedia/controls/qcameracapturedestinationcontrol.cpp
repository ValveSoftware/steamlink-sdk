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

#include <qcameracapturedestinationcontrol.h>
#include <QtCore/qstringlist.h>

QT_BEGIN_NAMESPACE

/*!
    \class QCameraCaptureDestinationControl

    \brief The QCameraCaptureDestinationControl class provides a control for setting capture destination.

    Depending on backend capabilities capture to file, buffer or both can be supported.

    \inmodule QtMultimedia

    \ingroup multimedia_control

    The interface name of QCameraCaptureDestinationControl is \c org.qt-project.qt.cameracapturedestinationcontrol/5.0 as
    defined in QCameraCaptureDestinationControl_iid.


    \sa QMediaService::requestControl()
*/

/*!
    \macro QCameraCaptureDestinationControl_iid

    \c org.qt-project.qt.cameracapturedestinationcontrol/5.0

    Defines the interface name of the QCameraCaptureDestinationControl class.

    \relates QCameraCaptureDestinationControl
*/

/*!
    Constructs a new image capture destination control object with the given \a parent
*/
QCameraCaptureDestinationControl::QCameraCaptureDestinationControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys an image capture destination control.
*/
QCameraCaptureDestinationControl::~QCameraCaptureDestinationControl()
{
}

/*!
    \fn QCameraCaptureDestinationControl::isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const

    Returns true if the capture \a destination is supported; and false if it is not.
*/

/*!
    \fn QCameraCaptureDestinationControl::captureDestination() const

    Returns the current capture destination. The default destination is QCameraImageCapture::CaptureToFile.
*/

/*!
    \fn QCameraCaptureDestinationControl::setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)

    Sets the capture \a destination.
*/

/*!
    \fn QCameraCaptureDestinationControl::captureDestinationChanged(QCameraImageCapture::CaptureDestinations destination)

    Signals the image capture \a destination changed.
*/

#include "moc_qcameracapturedestinationcontrol.cpp"
QT_END_NAMESPACE

