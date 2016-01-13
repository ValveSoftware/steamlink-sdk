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
