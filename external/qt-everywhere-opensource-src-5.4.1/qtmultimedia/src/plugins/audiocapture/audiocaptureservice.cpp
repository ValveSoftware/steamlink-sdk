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
