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

#include "qmediavideoprobecontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaVideoProbeControl
    \inmodule QtMultimedia


    \ingroup multimedia_control

    \brief The QMediaVideoProbeControl class allows control over probing video frames in media objects.

    \l QVideoProbe is the client facing class for probing video - this class is implemented by
    media backends to provide this functionality.

    The interface name of QMediaVideoProbeControl is \c org.qt-project.qt.mediavideoprobecontrol/5.0 as
    defined in QMediaVideoProbeControl_iid.

    \sa QVideoProbe, QMediaService::requestControl(), QMediaPlayer, QCamera
*/

/*!
    \macro QMediaVideoProbeControl_iid

    \c org.qt-project.qt.mediavideoprobecontrol/5.0

    Defines the interface name of the QMediaVideoProbeControl class.

    \relates QMediaVideoProbeControl
*/

/*!
  Create a new media video probe control object with the given \a parent.
*/
QMediaVideoProbeControl::QMediaVideoProbeControl(QObject *parent)
    : QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*! Destroys this video probe control */
QMediaVideoProbeControl::~QMediaVideoProbeControl()
{
}

/*!
    \fn QMediaVideoProbeControl::videoFrameProbed(const QVideoFrame &frame)

    This signal should be emitted when a video \a frame is processed in the
    media service.
*/

/*!
    \fn QMediaVideoProbeControl::flush()

    This signal should be emitted when it is required to release all frames.
*/

#include "moc_qmediavideoprobecontrol.cpp"

QT_END_NAMESPACE
