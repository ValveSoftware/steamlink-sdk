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

#include "qmediacontrol_p.h"
#include "qaudiorolecontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAudioRoleControl
    \inmodule QtMultimedia
    \ingroup multimedia_control
    \since 5.6

    \brief The QAudioRoleControl class provides control over the audio role of a media object.

    If a QMediaService supports audio roles it will implement QAudioRoleControl.

    The functionality provided by this control is exposed to application code through the
    QMediaPlayer class.

    The interface name of QAudioRoleControl is \c org.qt-project.qt.audiorolecontrol/5.6 as
    defined in QAudioRoleControl_iid.

    \sa QMediaService::requestControl(), QMediaPlayer
*/

/*!
    \macro QAudioRoleControl_iid

    \c org.qt-project.qt.audiorolecontrol/5.6

    Defines the interface name of the QAudioRoleControl class.

    \relates QAudioRoleControl
*/

/*!
    Construct a QAudioRoleControl with the given \a parent.
*/
QAudioRoleControl::QAudioRoleControl(QObject *parent)
    : QMediaControl(*new QMediaControlPrivate, parent)
{

}

/*!
    Destroys the audio role control.
*/
QAudioRoleControl::~QAudioRoleControl()
{

}

/*!
    \fn QAudio::Role QAudioRoleControl::audioRole() const

    Returns the audio role of the media played by the media service.
*/

/*!
    \fn void QAudioRoleControl::setAudioRole(QAudio::Role role)

    Sets the audio \a role of the media played by the media service.
*/

/*!
    \fn QList<QAudio::Role> QAudioRoleControl::supportedAudioRoles() const

    Returns a list of audio roles that the media service supports.
*/

/*!
    \fn void QAudioRoleControl::audioRoleChanged(QAudio::Role role)

    Signal emitted when the audio \a role has changed.
 */


#include "moc_qaudiorolecontrol.cpp"
QT_END_NAMESPACE
