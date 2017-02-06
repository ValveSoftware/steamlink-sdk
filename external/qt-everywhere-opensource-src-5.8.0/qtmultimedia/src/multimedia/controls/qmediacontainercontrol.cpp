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


#include "qmediacontainercontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaContainerControl

    \brief The QMediaContainerControl class provides access to the output container format of a QMediaService

    \inmodule QtMultimedia


    \ingroup multimedia_control

    If a QMediaService supports writing encoded data it will implement
    QMediaContainerControl.  This control provides information about the output
    containers supported by a media service and allows one to be selected as
    the current output containers.

    The functionality provided by this control is exposed to application code
    through the QMediaRecorder class.

    The interface name of QMediaContainerControl is \c org.qt-project.qt.mediacontainercontrol/5.0 as
    defined in QMediaContainerControl_iid.

    \sa QMediaService::requestControl(), QMediaRecorder
*/

/*!
    \macro QMediaContainerControl_iid

    \c org.qt-project.qt.mediacontainercontrol/5.0

    Defines the interface name of the QMediaContainerControl class.

    \relates QMediaContainerControl
*/

/*!
    Constructs a new media container control with the given \a parent.
*/
QMediaContainerControl::QMediaContainerControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys a media container control.
*/
QMediaContainerControl::~QMediaContainerControl()
{
}


/*!
    \fn QMediaContainerControl::supportedContainers() const

    Returns a list of MIME types of supported container formats.
*/

/*!
    \fn QMediaContainerControl::containerFormat() const

    Returns the selected container format.
*/

/*!
    \fn QMediaContainerControl::setContainerFormat(const QString &format)

    Sets the current container \a format.
*/

/*!
    \fn QMediaContainerControl::containerDescription(const QString &format) const

    Returns a description of the container \a format.
*/

#include "moc_qmediacontainercontrol.cpp"
QT_END_NAMESPACE

