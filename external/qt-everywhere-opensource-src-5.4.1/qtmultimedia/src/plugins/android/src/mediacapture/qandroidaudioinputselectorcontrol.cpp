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
