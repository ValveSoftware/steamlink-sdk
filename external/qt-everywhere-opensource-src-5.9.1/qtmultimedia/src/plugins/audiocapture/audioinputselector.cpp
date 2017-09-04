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

#include "audiocapturesession.h"
#include "audioinputselector.h"

#include <qaudiodeviceinfo.h>

QT_BEGIN_NAMESPACE

AudioInputSelector::AudioInputSelector(QObject *parent)
    :QAudioInputSelectorControl(parent)
{
    m_session = qobject_cast<AudioCaptureSession*>(parent);

    update();

    m_audioInput = defaultInput();
}

AudioInputSelector::~AudioInputSelector()
{
}

QList<QString> AudioInputSelector::availableInputs() const
{
    return m_names;
}

QString AudioInputSelector::inputDescription(const QString& name) const
{
    QString desc;

    for(int i = 0; i < m_names.count(); i++) {
        if (m_names.at(i).compare(name) == 0) {
            desc = m_names.at(i);
            break;
        }
    }
    return desc;
}

QString AudioInputSelector::defaultInput() const
{
    return QAudioDeviceInfo::defaultInputDevice().deviceName();
}

QString AudioInputSelector::activeInput() const
{
    return m_audioInput;
}

void AudioInputSelector::setActiveInput(const QString& name)
{
    if (m_audioInput.compare(name) != 0) {
        m_audioInput = name;
        m_session->setCaptureDevice(name);
        emit activeInputChanged(name);
    }
}

void AudioInputSelector::update()
{
    m_names.clear();
    m_descriptions.clear();

    QList<QAudioDeviceInfo> devices;
    devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for(int i = 0; i < devices.size(); ++i) {
        m_names.append(devices.at(i).deviceName());
        m_descriptions.append(devices.at(i).deviceName());
    }
}

QT_END_NAMESPACE
