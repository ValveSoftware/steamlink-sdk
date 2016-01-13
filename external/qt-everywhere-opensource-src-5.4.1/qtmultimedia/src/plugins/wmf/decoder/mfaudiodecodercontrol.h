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

#ifndef MFAUDIODECODERCONTROL_H
#define MFAUDIODECODERCONTROL_H

#include "qaudiodecodercontrol.h"
#include "mfdecodersourcereader.h"
#include "sourceresolver.h"

QT_USE_NAMESPACE

class MFAudioDecoderControl : public QAudioDecoderControl
{
    Q_OBJECT
public:
    MFAudioDecoderControl(QObject *parent = 0);
    ~MFAudioDecoderControl();

    QAudioDecoder::State state() const;

    QString sourceFilename() const;
    void setSourceFilename(const QString &fileName);

    QIODevice* sourceDevice() const;
    void setSourceDevice(QIODevice *device);

    void start();
    void stop();

    QAudioFormat audioFormat() const;
    void setAudioFormat(const QAudioFormat &format);

    QAudioBuffer read();
    bool bufferAvailable() const;

    qint64 position() const;
    qint64 duration() const;

private Q_SLOTS:
    void handleMediaSourceReady();
    void handleMediaSourceError(long hr);
    void handleSampleAdded();
    void handleSourceFinished();

private:
    void updateResamplerOutputType();
    void activatePipeline();
    void onSourceCleared();

    MFDecoderSourceReader  *m_decoderSourceReader;
    SourceResolver         *m_sourceResolver;
    IMFTransform           *m_resampler;
    QAudioDecoder::State    m_state;
    QString                 m_sourceFilename;
    QIODevice              *m_device;
    QAudioFormat            m_audioFormat;
    DWORD                   m_mfInputStreamID;
    DWORD                   m_mfOutputStreamID;
    bool                    m_bufferReady;
    QAudioBuffer            m_cachedAudioBuffer;
    qint64                  m_duration;
    qint64                  m_position;
    bool                    m_loadingSource;
    IMFMediaType           *m_mfOutputType;
    IMFSample              *m_convertSample;
    QAudioFormat            m_sourceOutputFormat;
    bool                    m_sourceReady;
    bool                    m_resamplerDirty;
};

#endif//MFAUDIODECODERCONTROL_H
