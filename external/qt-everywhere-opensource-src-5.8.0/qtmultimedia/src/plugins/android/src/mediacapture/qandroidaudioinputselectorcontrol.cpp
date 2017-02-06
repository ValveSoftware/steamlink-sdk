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

#include "qandroidaudioinputselectorcontrol.h"

#include "qandroidcapturesession.h"

QT_BEGIN_NAMESPACE

QAndroidAudioInputSelectorControl::QAndroidAudioInputSelectorControl(QAndroidCaptureSession *session)
    : QAudioInputSelectorControl()
    , m_session(session)
{
    connect(m_session, SIGNAL(audioInputChanged(QString)), this, SIGNAL(activeInputChanged(QString)));
}

QList<QString> QAndroidAudioInputSelectorControl::availableInputs() const
{
    return QList<QString>() << QLatin1String("default")
                            << QLatin1String("mic")
                            << QLatin1String("voice_uplink")
                            << QLatin1String("voice_downlink")
                            << QLatin1String("voice_call")
                            << QLatin1String("voice_recognition");
}

QString QAndroidAudioInputSelectorControl::inputDescription(const QString& name) const
{
    return availableDeviceDescription(name.toLatin1());
}

QString QAndroidAudioInputSelectorControl::defaultInput() const
{
    return QLatin1String("default");
}

QString QAndroidAudioInputSelectorControl::activeInput() const
{
    return m_session->audioInput();
}

void QAndroidAudioInputSelectorControl::setActiveInput(const QString& name)
{
    m_session->setAudioInput(name);
}

QList<QByteArray> QAndroidAudioInputSelectorControl::availableDevices()
{
    return QList<QByteArray>() << "default"
                               << "mic"
                               << "voice_uplink"
                               << "voice_downlink"
                               << "voice_call"
                               << "voice_recognition";
}

QString QAndroidAudioInputSelectorControl::availableDeviceDescription(const QByteArray &device)
{
    if (device == "default")
        return QLatin1String("Default audio source");
    else if (device == "mic")
        return QLatin1String("Microphone audio source");
    else if (device == "voice_uplink")
        return QLatin1String("Voice call uplink (Tx) audio source");
    else if (device == "voice_downlink")
        return QLatin1String("Voice call downlink (Rx) audio source");
    else if (device == "voice_call")
        return QLatin1String("Voice call uplink + downlink audio source");
    else if (device == "voice_recognition")
        return QLatin1String("Microphone audio source tuned for voice recognition");
    else
        return QString();
}

QT_END_NAMESPACE
