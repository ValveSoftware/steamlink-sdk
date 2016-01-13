/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef MOCKAUDIOENCODERCONTROL_H
#define MOCKAUDIOENCODERCONTROL_H

#include "qaudioencodersettingscontrol.h"

class MockAudioEncoderControl : public QAudioEncoderSettingsControl
{
    Q_OBJECT
public:
    MockAudioEncoderControl(QObject *parent):
        QAudioEncoderSettingsControl(parent)
    {
        m_codecs << "audio/pcm" << "audio/mpeg";
        m_descriptions << "Pulse Code Modulation" << "mp3 format";
        m_audioSettings.setCodec("audio/pcm");
        m_audioSettings.setBitRate(128*1024);
        m_audioSettings.setSampleRate(8000);
        m_freqs << 8000 << 11025 << 22050 << 44100;
    }

    ~MockAudioEncoderControl() {}

    QAudioEncoderSettings audioSettings() const
    {
        return m_audioSettings;
    }

    void setAudioSettings(const QAudioEncoderSettings &settings)
    {
        m_audioSettings = settings;
    }

    QList<int> supportedChannelCounts(const QAudioEncoderSettings & = QAudioEncoderSettings()) const
    {
        QList<int> list; list << 1 << 2; return list;
    }

    QList<int> supportedSampleRates(const QAudioEncoderSettings & = QAudioEncoderSettings(), bool *continuous = 0) const
    {
        if (continuous)
            *continuous = false;

        return m_freqs;
    }

    QStringList supportedAudioCodecs() const
    {
        return m_codecs;
    }

    QString codecDescription(const QString &codecName) const
    {
        return m_descriptions.value(m_codecs.indexOf(codecName));
    }

private:
    QAudioEncoderSettings m_audioSettings;

    QStringList  m_codecs;
    QStringList  m_descriptions;

    QList<int>   m_freqs;

};

#endif // MOCKAUDIOENCODERCONTROL_H
