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



#include "qcamerafeedbackcontrol.h"
#include <private/qmediacontrol_p.h>

/*!
    \class QCameraFeedbackControl

    \brief The QCameraFeedbackControl class allows controlling feedback (sounds etc) during camera operation

    \inmodule QtMultimedia

    \ingroup multimedia_control
    \since 5.0

    When using a camera, there are several times when some form of feedback to
    the user is given - for example, when an image is taken, or when recording is started.
    You can enable or disable some of this feedback, or adjust what sound might be played
    for these actions.

    In some cases it may be undesirable to play a sound effect - for example, when initiating
    video recording the sound itself may be recorded.

    \note In some countries or regions, feedback sounds or other indications (e.g. a red light) are
    mandatory during camera operation.  In these cases, you can check \c isEventFeedbackLocked to check
    if that type of feedback can be modified.  Any attempts to change a locked feedback type will be
    ignored.

    The interface name of QCameraFeedbackControl is \c org.qt-project.qt.camerafeedbackcontrol/5.0 as
    defined in QCameraFeedbackControl_iid.

    \sa QCamera
*/

/*!
    \enum QCameraFeedbackControl::EventType

    This enumeration describes certain events that occur during camera usage.  You
    can associate some form of feedback to be given when the event occurs, or check
    whether feedback for this event is enabled or locked so that changes cannot be made.



    \value ViewfinderStarted    The viewfinder stream was started (even if not visible)
    \value ViewfinderStopped    The viewfinder stream was stopped
    \value ImageCaptured        An image was captured but not yet fully processed
    \value ImageSaved           An image is fully available and saved somewhere.
    \value ImageError           An error occurred while capturing an image
    \value RecordingStarted     Video recording has started
    \value RecordingInProgress  Video recording is in progress
    \value RecordingStopped     Video recording has stopped
    \value AutoFocusInProgress  The camera is trying to automatically focus
    \value AutoFocusLocked      The camera has automatically focused successfully
    \value AutoFocusFailed      The camera was unable to focus automatically
*/

/*!
    \macro QCameraFeedbackControl_iid

    \c org.qt-project.qt.camerafeedbackcontrol/5.0

    Defines the interface name of the QCameraFeedbackControl class.

    \relates QCameraFeedbackControl
*/

/*!
    Constructs a camera feedback control object with \a parent.
*/
QCameraFeedbackControl::QCameraFeedbackControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys the camera feedback control object.
*/
QCameraFeedbackControl::~QCameraFeedbackControl()
{
}

/*!
  \fn bool QCameraFeedbackControl::isEventFeedbackLocked(EventType event) const

  Returns true if the feedback setting for \a event is locked.  This may be true
  because of legal compliance issues, or because configurability of this event's
  feedback is not supported.

  \since 5.0
*/

/*!
  \fn bool QCameraFeedbackControl::isEventFeedbackEnabled(EventType event) const

  Returns true if the feedback for \a event is enabled.

  \since 5.0
*/

/*!
  \fn bool QCameraFeedbackControl::setEventFeedbackEnabled(EventType event, bool enabled)

  Turns on feedback for the specific \a event if \a enabled is true, otherwise disables the
  feedback.  Returns true if the feedback could be modified, or false otherwise (e.g. this feedback
  type is locked).

  \since 5.0
*/


/*!
  \fn void QCameraFeedbackControl::resetEventFeedback(EventType event)

  Restores the feedback setting for this \a event to its default setting.

  \since 5.0
*/

/*!
  \fn bool QCameraFeedbackControl::setEventFeedbackSound(EventType event, const QString &filePath)

  When the given \a event occurs, the sound effect referenced by \a filePath
  will be played instead of the default sound.

  If this feedback type is locked, or if the supplied path is inaccessible,
  this function will return false.  In addition, some forms of feedback may
  be non-auditory (e.g. a red light, or a vibration), and false may be
  returned in this case.

  The file referenced should be linear PCM (WAV format).

  \note In the case that a valid file path to an unsupported file is given, this
  function will return true but the feedback will use the original setting.

  \since 5.0
*/






