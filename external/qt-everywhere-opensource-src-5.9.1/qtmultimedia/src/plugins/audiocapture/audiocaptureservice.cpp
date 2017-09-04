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

#include "audiocaptureservice.h"
#include "audiocapturesession.h"
#include "audioinputselector.h"
#include "audioencodercontrol.h"
#include "audiocontainercontrol.h"
#include "audiomediarecordercontrol.h"
#include "audiocaptureprobecontrol.h"

QT_BEGIN_NAMESPACE

AudioCaptureService::AudioCaptureService(QObject *parent):
    QMediaService(parent)
{
    m_session = new AudioCaptureSession(this);
    m_encoderControl  = new AudioEncoderControl(m_session);
    m_containerControl = new AudioContainerControl(m_session);
    m_mediaControl   = new AudioMediaRecorderControl(m_session);
    m_inputSelector  = new AudioInputSelector(m_session);
}

AudioCaptureService::~AudioCaptureService()
{
    delete m_encoderControl;
    delete m_containerControl;
    delete m_inputSelector;
    delete m_mediaControl;
    delete m_session;
}

QMediaControl *AudioCaptureService::requestControl(const char *name)
{
    if (qstrcmp(name,QMediaRecorderControl_iid) == 0)
        return m_mediaControl;

    if (qstrcmp(name,QAudioEncoderSettingsControl_iid) == 0)
        return m_encoderControl;

    if (qstrcmp(name,QAudioInputSelectorControl_iid) == 0)
        return m_inputSelector;

    if (qstrcmp(name,QMediaContainerControl_iid) == 0)
        return m_containerControl;

    if (qstrcmp(name,QMediaAudioProbeControl_iid) == 0) {
        AudioCaptureProbeControl *probe = new AudioCaptureProbeControl(this);
        m_session->addProbe(probe);
        return probe;
    }

    return 0;
}

void AudioCaptureService::releaseControl(QMediaControl *control)
{
    Q_UNUSED(control)
}

QT_END_NAMESPACE
