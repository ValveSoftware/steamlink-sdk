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

#include "qaudioencodersettingscontrol.h"
#include <QtCore/qstringlist.h>

QT_BEGIN_NAMESPACE


/*!
    \class QAudioEncoderSettingsControl
    \inmodule QtMultimedia

    \ingroup multimedia_control

    \brief The QAudioEncoderSettingsControl class provides access to the settings of a
    media service that performs audio encoding.

    If a QMediaService supports encoding audio data it will implement
    QAudioEncoderSettingsControl.  This control provides information about the limits
    of restricted audio encoder options and allows the selection of a set of
    audio encoder settings as specified in a QAudioEncoderSettings object.

    The functionality provided by this control is exposed to application code through the
    QMediaRecorder class.

    The interface name of QAudioEncoderSettingsControl is \c org.qt-project.qt.audioencodersettingscontrol/5.0 as
    defined in QAudioEncoderSettingsControl_iid.

    \sa QMediaService::requestControl(), QMediaRecorder
*/

/*!
    \macro QAudioEncoderSettingsControl_iid

    \c org.qt-project.qt.audioencodersettingscontrol/5.0

    Defines the interface name of the QAudioEncoderSettingsControl class.

    \relates QAudioEncoderSettingsControl
*/

/*!
  Create a new audio encoder settings control object with the given \a parent.
*/
QAudioEncoderSettingsControl::QAudioEncoderSettingsControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
  Destroys the audio encoder settings control.
*/
QAudioEncoderSettingsControl::~QAudioEncoderSettingsControl()
{
}

/*!
  \fn QAudioEncoderSettingsControl::supportedAudioCodecs() const

  Returns the list of supported audio codec names.
*/

/*!
  \fn QAudioEncoderSettingsControl::codecDescription(const QString &codec) const

  Returns description of audio \a codec.
*/

/*!
  \fn QAudioEncoderSettingsControl::supportedSampleRates(const QAudioEncoderSettings &settings = QAudioEncoderSettings(),
                                                 bool *continuous) const

  Returns the list of supported audio sample rates, if known.

  If non null audio \a settings parameter is passed,
  the returned list is reduced to sample rates supported with partial settings applied.

  It can be used for example to query the list of sample rates, supported by specific audio codec.

  If the encoder supports arbitrary sample rates within the supported rates range,
  *\a continuous is set to true, otherwise *\a continuous is set to false.
*/

/*!
    \fn QAudioEncoderSettingsControl::audioSettings() const

    Returns the audio encoder settings.

    The returned value may be different tha passed to QAudioEncoderSettingsControl::setAudioSettings()
    if the settings contains the default or undefined parameters.
    In this case if the undefined parameters are already resolved, they should be returned.
*/

/*!
    \fn QAudioEncoderSettingsControl::setAudioSettings(const QAudioEncoderSettings &settings)

    Sets the selected audio \a settings.
*/

#include "moc_qaudioencodersettingscontrol.cpp"
QT_END_NAMESPACE

