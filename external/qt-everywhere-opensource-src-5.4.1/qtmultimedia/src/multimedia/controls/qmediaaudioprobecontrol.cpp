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

#include "qmediaaudioprobecontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaAudioProbeControl
    \inmodule QtMultimedia


    \ingroup multimedia_control

    \brief The QMediaAudioProbeControl class allows control over probing audio data in media objects.

    \l QAudioProbe is the client facing class for probing audio - this class is implemented by
    media backends to provide this functionality.

    The interface name of QMediaAudioProbeControl is \c org.qt-project.qt.mediaaudioprobecontrol/5.0 as
    defined in QMediaAudioProbeControl_iid.

    \sa QAudioProbe, QMediaService::requestControl(), QMediaPlayer, QCamera
*/

/*!
    \macro QMediaAudioProbeControl_iid

    \c org.qt-project.qt.mediaaudioprobecontrol/5.0

    Defines the interface name of the QMediaAudioProbeControl class.

    \relates QMediaAudioProbeControl
*/

/*!
  Create a new media audio probe control object with the given \a parent.
*/
QMediaAudioProbeControl::QMediaAudioProbeControl(QObject *parent)
    : QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*! Destroys this audio probe control */
QMediaAudioProbeControl::~QMediaAudioProbeControl()
{
}

/*!
    \fn QMediaAudioProbeControl::audioBufferProbed(const QAudioBuffer &buffer)

    This signal should be emitted when an audio \a buffer is processed in the
    media service.
*/


/*!
    \fn QMediaAudioProbeControl::flush()

    This signal should be emitted when it is required to release all frames.
*/

#include "moc_qmediaaudioprobecontrol.cpp"

QT_END_NAMESPACE
