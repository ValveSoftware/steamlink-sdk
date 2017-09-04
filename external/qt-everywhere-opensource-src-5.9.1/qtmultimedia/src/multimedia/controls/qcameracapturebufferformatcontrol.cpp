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

