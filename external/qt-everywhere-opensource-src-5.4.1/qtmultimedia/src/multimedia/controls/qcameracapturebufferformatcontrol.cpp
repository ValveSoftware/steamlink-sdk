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

#include <qcameracapturebufferformatcontrol.h>

QT_BEGIN_NAMESPACE

/*!
    \class QCameraCaptureBufferFormatControl

    \brief The QCameraCaptureBufferFormatControl class provides a control for setting the capture buffer format.

    The format is of type QVideoFrame::PixelFormat.

    \inmodule QtMultimedia

    \ingroup multimedia_control

    The interface name of QCameraCaptureBufferFormatControl is \c org.qt-project.qt.cameracapturebufferformatcontrol/5.0 as
    defined in QCameraCaptureBufferFormatControl_iid.

    \sa QMediaService::requestControl()
*/

/*!
    \macro QCameraCaptureBufferFormatControl_iid

    \c org.qt-project.qt.cameracapturebufferformatcontrol/5.0

    Defines the interface name of the QCameraCaptureBufferFormatControl class.

    \relates QCameraCaptureBufferFormatControl
*/

/*!
    Constructs a new image buffer capture format control object with the given \a parent
*/
QCameraCaptureBufferFormatControl::QCameraCaptureBufferFormatControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys an image buffer capture format control.
*/
QCameraCaptureBufferFormatControl::~QCameraCaptureBufferFormatControl()
{
}

/*!
    \fn QCameraCaptureBufferFormatControl::supportedBufferFormats() const

    Returns the list of the supported buffer capture formats.
*/

/*!
    \fn QCameraCaptureBufferFormatControl::bufferFormat() const

    Returns the current buffer capture format.
*/

/*!
    \fn QCameraCaptureBufferFormatControl::setBufferFormat(QVideoFrame::PixelFormat format)

    Sets the buffer capture \a format.
*/

/*!
    \fn QCameraCaptureBufferFormatControl::bufferFormatChanged(QVideoFrame::PixelFormat format)

    Signals the buffer image capture format changed to \a format.
*/

#include "moc_qcameracapturebufferformatcontrol.cpp"
QT_END_NAMESPACE

