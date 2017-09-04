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

#include "Wmcodecdsp.h"
#include "mfaudiodecodercontrol.h"

MFAudioDecoderControl::MFAudioDecoderControl(QObject *parent)
    : QAudioDecoderControl(parent)
    , m_decoderSourceReader(new MFDecoderSourceReader)
    , m_sourceResolver(new SourceResolver)
    , m_resampler(0)
    , m_state(QAudioDecoder::StoppedState)
    , m_device(0)
    , m_mfInputStreamID(0)
    , m_mfOutputStreamID(0)
    , m_bufferReady(false)
    , m_duration(0)
    , m_position(0)
    , m_loadingSource(false)
    , m_mfOutputType(0)
    , m_convertSample(0)
    , m_sourceReady(false)
    , m_resamplerDirty(false)
{
    CoCreateInstance(CLSID_CResamplerMediaObject, NULL, CLSCTX_INPROC_SERVER, IID_IMFTransform, (LPVOID*)(&m_resampler));
    if (!m_resampler) {
        qCritical("MFAudioDecoderControl: Failed to create resampler(CLSID_CResamplerMediaObject)!");
        return;
    }
    m_resampler->AddInputStreams(1, &m_mfInputStreamID);

    connect(m_sourceResolver, SIGNAL(mediaSourceReady()), this, SLOT(handleMediaSourceReady()));
    connect(m_sourceResolver, SIGNAL(error(long)), this, SLOT(handleMediaSourceError(long)));
    connect(m_decoderSourceReader, SIGNAL(finished()), this, SLOT(handleSourceFinished()));

    QAudioFormat defaultFormat;
    defaultFormat.setCodec("audio/pcm");
    setAudioFormat(defaultFormat);
}

MFAudioDecoderControl::~MFAudioDecoderControl()
{
    if (m_mfOutputType)
        m_mfOutputType->Release();
    m_decoderSourceReader->shutdown();
    m_decoderSourceReader->Release();
    m_sourceResolver->Release();
    if (m_resampler)
        m_resampler->Release();
}

QAudioDecoder::State MFAudioDecoderControl::state() const
{
    return m_state;
}

QString MFAudioDecoderControl::sourceFilename() const
{
    return m_sourceFilename;
}

void MFAudioDecoderControl::onSourceCleared()
{
    bool positionDirty = false;
    bool durationDirty = false;
    if (m_position != 0) {
        m_position = 0;
        positionDirty = true;
    }
    if (m_duration != 0) {
        m_duration = 0;
        durationDirty = true;
    }
    if (positionDirty)
        emit positionChanged(m_position);
    if (durationDirty)
        emit durationChanged(m_duration);
}

void MFAudioDecoderControl::setSourceFilename(const QString &fileName)
{
    if (!m_device && m_sourceFilename == fileName)
        return;
    m_sourceReady = false;
    m_sourceResolver->cancel();
    m_decoderSourceReader->setSource(0, m_audioFormat);
    m_device = 0;
    m_sourceFilename = fileName;
    if (!m_sourceFilename.isEmpty()) {
        m_sourceResolver->shutdown();
        QMediaResourceList rl;
        QUrl url;
        if (m_sourceFilename.startsWith(':'))
            url = QUrl(QStringLiteral("qrc%1").arg(m_sourceFilename));
        else
            url = QUrl::fromLocalFile(m_sourceFilename);
        rl.push_back(QMediaResource(url));
        m_sourceResolver->load(rl, 0);
        m_loadingSource = true;
    } else {
        onSourceCleared();
    }
    emit sourceChanged();
}

QIODevice* MFAudioDecoderControl::sourceDevice() const
{
    return m_device;
}

void MFAudioDecoderControl::setSourceDevice(QIODevice *device)
{
    if (m_device == device && m_sourceFilename.isEmpty())
        return;
    m_sourceReady = false;
    m_sourceResolver->cancel();
    m_decoderSourceReader->setSource(0, m_audioFormat);
    m_sourceFilename.clear();
    m_device = device;
    if (m_device) {
        m_sourceResolver->shutdown();
        m_sourceResolver->load(QMediaResourceList(), m_device);
        m_loadingSource = true;
    } else {
        onSourceCleared();
    }
    emit sourceChanged();
}

void MFAudioDecoderControl::updateResamplerOutputType()
{
    m_resamplerDirty = false;
    if (m_audioFormat == m_sourceOutputFormat)
        return;
    HRESULT hr = m_resampler->SetOutputType(m_mfOutputStreamID, m_mfOutputType, 0);
    if (SUCCEEDED(hr)) {
        MFT_OUTPUT_STREAM_INFO streamInfo;
        m_resampler->GetOutputStreamInfo(m_mfOutputStreamID, &streamInfo);
        if ((streamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |  MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) == 0) {
            //if resampler does not allocate output sample memory, we do it here
            if (m_convertSample) {
                m_convertSample->Release();
                m_convertSample = 0;
            }
            if (SUCCEEDED(MFCreateSample(&m_convertSample))) {
                IMFMediaBuffer *mbuf = 0;;
                if (SUCCEEDED(MFCreateMemoryBuffer(streamInfo.cbSize, &mbuf))) {
                    m_convertSample->AddBuffer(mbuf);
                    mbuf->Release();
                }
            }
        }
    } else {
        qWarning() << "MFAudioDecoderControl: failed to SetOutputType of resampler" << hr;
    }
}

void MFAudioDecoderControl::handleMediaSourceReady()
{
    m_loadingSource = false;
    m_sourceReady = true;
    IMFMediaType *mediaType = m_decoderSourceReader->setSource(m_sourceResolver->mediaSource(), m_audioFormat);
    m_sourceOutputFormat = QAudioFormat();

    if (mediaType) {
        m_sourceOutputFormat = m_audioFormat;
        QAudioFormat af = m_audioFormat;

        UINT32 val = 0;
        if (SUCCEEDED(mediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &val))) {
            m_sourceOutputFormat.setChannelCount(int(val));
        }
        if (SUCCEEDED(mediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &val))) {
            m_sourceOutputFormat.setSampleRate(int(val));
        }
        if (SUCCEEDED(mediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &val))) {
            m_sourceOutputFormat.setSampleSize(int(val));
        }

        GUID subType;
        if (SUCCEEDED(mediaType->GetGUID(MF_MT_SUBTYPE, &subType))) {
            if (subType == MFAudioFormat_Float) {
                m_sourceOutputFormat.setSampleType(QAudioFormat::Float);
            } else if (m_sourceOutputFormat.sampleSize() == 8) {
                m_sourceOutputFormat.setSampleType(QAudioFormat::UnSignedInt);
            } else {
                m_sourceOutputFormat.setSampleType(QAudioFormat::SignedInt);
            }
        }
        if (m_sourceOutputFormat.sampleType() != QAudioFormat::Float) {
            m_sourceOutputFormat.setByteOrder(QAudioFormat::LittleEndian);
        }

        if (m_audioFormat.sampleType() != QAudioFormat::Float
            && m_audioFormat.sampleType() != QAudioFormat::SignedInt) {
            af.setSampleType(m_sourceOutputFormat.sampleType());
        }
        if (af.sampleType() == QAudioFormat::SignedInt) {
            af.setByteOrder(QAudioFormat::LittleEndian);
        }
        if (m_audioFormat.channelCount() <= 0) {
            af.setChannelCount(m_sourceOutputFormat.channelCount());
        }
        if (m_audioFormat.sampleRate() <= 0) {
            af.setSampleRate(m_sourceOutputFormat.sampleRate());
        }
        if (m_audioFormat.sampleSize() <= 0) {
            af.setSampleSize(m_sourceOutputFormat.sampleSize());
        }
        setAudioFormat(af);
    }

    if (m_sourceResolver->mediaSource()) {
        if (mediaType && m_resampler) {
            HRESULT hr = S_OK;
            hr = m_resampler->SetInputType(m_mfInputStreamID, mediaType, 0);
            if (SUCCEEDED(hr)) {
                updateResamplerOutputType();
            } else {
                qWarning() << "MFAudioDecoderControl: failed to SetInputType of resampler" << hr;
            }
        }
        IMFPresentationDescriptor *pd;
        if (SUCCEEDED(m_sourceResolver->mediaSource()->CreatePresentationDescriptor(&pd))) {
            UINT64 duration = 0;
            pd->GetUINT64(MF_PD_DURATION, &duration);
            pd->Release();
            duration /= 10000;
            if (m_duration != qint64(duration)) {
                m_duration = qint64(duration);
                emit durationChanged(m_duration);
            }
        }
        if (m_state == QAudioDecoder::DecodingState) {
            activatePipeline();
        }
    } else if (m_state != QAudioDecoder::StoppedState) {
        m_state = QAudioDecoder::StoppedState;
        emit stateChanged(m_state);
    }
}

void MFAudioDecoderControl::handleMediaSourceError(long hr)
{
    Q_UNUSED(hr);
    m_loadingSource = false;
    m_decoderSourceReader->setSource(0, m_audioFormat);
    if (m_state != QAudioDecoder::StoppedState) {
        m_state = QAudioDecoder::StoppedState;
        emit stateChanged(m_state);
    }
}

void MFAudioDecoderControl::activatePipeline()
{
    Q_ASSERT(!m_bufferReady);
    m_state = QAudioDecoder::DecodingState;
    connect(m_decoderSourceReader, SIGNAL(sampleAdded()), this, SLOT(handleSampleAdded()));
    if (m_resamplerDirty) {
        updateResamplerOutputType();
    }
    m_decoderSourceReader->reset();
    m_decoderSourceReader->readNextSample();
    if (m_position != 0) {
        m_position = 0;
        emit positionChanged(0);
    }
}

void MFAudioDecoderControl::start()
{
    if (m_state != QAudioDecoder::StoppedState)
        return;

    if (m_loadingSource) {
        //deferred starting
        m_state = QAudioDecoder::DecodingState;
        emit stateChanged(m_state);
        return;
    }

    if (!m_decoderSourceReader->mediaSource())
        return;
    activatePipeline();
    emit stateChanged(m_state);
}

void MFAudioDecoderControl::stop()
{
    if (m_state == QAudioDecoder::StoppedState)
        return;
    m_state = QAudioDecoder::StoppedState;
    disconnect(m_decoderSourceReader, SIGNAL(sampleAdded()), this, SLOT(handleSampleAdded()));
    if (m_bufferReady) {
        m_bufferReady = false;
        emit bufferAvailableChanged(m_bufferReady);
    }
    emit stateChanged(m_state);
}

void MFAudioDecoderControl::handleSampleAdded()
{
    QList<IMFSample*> samples = m_decoderSourceReader->takeSamples();
    Q_ASSERT(samples.count() > 0);
    Q_ASSERT(!m_bufferReady);
    Q_ASSERT(m_resampler);
    LONGLONG sampleStartTime = 0;
    IMFSample *firstSample = samples.first();
    firstSample->GetSampleTime(&sampleStartTime);
    QByteArray abuf;
    if (m_sourceOutputFormat == m_audioFormat) {
        //no need for resampling
         for (IMFSample *s : qAsConst(samples)) {
            IMFMediaBuffer *buffer;
            s->ConvertToContiguousBuffer(&buffer);
            DWORD bufLen = 0;
            BYTE *buf = 0;
            if (SUCCEEDED(buffer->Lock(&buf, NULL, &bufLen))) {
                abuf.push_back(QByteArray(reinterpret_cast<char*>(buf), bufLen));
                buffer->Unlock();
            }
            buffer->Release();
            LONGLONG sampleTime = 0, sampleDuration = 0;
            s->GetSampleTime(&sampleTime);
            s->GetSampleDuration(&sampleDuration);
            m_position = qint64(sampleTime + sampleDuration) / 10000;
            s->Release();
        }
    } else {
        for (IMFSample *s : qAsConst(samples)) {
            HRESULT hr = m_resampler->ProcessInput(m_mfInputStreamID, s, 0);
            if (SUCCEEDED(hr)) {
                MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
                outputDataBuffer.dwStreamID = m_mfOutputStreamID;
                while (true) {
                    outputDataBuffer.pEvents = 0;
                    outputDataBuffer.dwStatus = 0;
                    outputDataBuffer.pSample = m_convertSample;
                    DWORD status = 0;
                    if (SUCCEEDED(m_resampler->ProcessOutput(0, 1, &outputDataBuffer, &status))) {
                        IMFMediaBuffer *buffer;
                        outputDataBuffer.pSample->ConvertToContiguousBuffer(&buffer);
                        DWORD bufLen = 0;
                        BYTE *buf = 0;
                        if (SUCCEEDED(buffer->Lock(&buf, NULL, &bufLen))) {
                            abuf.push_back(QByteArray(reinterpret_cast<char*>(buf), bufLen));
                            buffer->Unlock();
                        }
                        buffer->Release();
                    } else {
                        break;
                    }
                }
            }
            LONGLONG sampleTime = 0, sampleDuration = 0;
            s->GetSampleTime(&sampleTime);
            s->GetSampleDuration(&sampleDuration);
            m_position = qint64(sampleTime + sampleDuration) / 10000;
            s->Release();
        }
    }
    // WMF uses 100-nanosecond units, QAudioDecoder uses milliseconds, QAudioBuffer uses microseconds...
    m_cachedAudioBuffer = QAudioBuffer(abuf, m_audioFormat, qint64(sampleStartTime / 10));
    m_bufferReady = true;
    emit positionChanged(m_position);
    emit bufferAvailableChanged(m_bufferReady);
    emit bufferReady();
}

void MFAudioDecoderControl::handleSourceFinished()
{
    stop();
    emit finished();
}

QAudioFormat MFAudioDecoderControl::audioFormat() const
{
    return m_audioFormat;
}

void MFAudioDecoderControl::setAudioFormat(const QAudioFormat &format)
{
    if (m_audioFormat == format || !m_resampler)
        return;
    if (format.codec() != QLatin1String("audio/x-wav") && format.codec() != QLatin1String("audio/pcm")) {
        qWarning("MFAudioDecoderControl does not accept non-pcm audio format!");
        return;
    }
    m_audioFormat = format;

    if (m_audioFormat.isValid()) {
        IMFMediaType *mediaType = 0;
        MFCreateMediaType(&mediaType);
        mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (format.sampleType() == QAudioFormat::Float) {
            mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        } else {
            mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        }

        mediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, UINT32(m_audioFormat.channelCount()));
        mediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, UINT32(m_audioFormat.sampleRate()));
        UINT32 alignmentBlock = UINT32(m_audioFormat.channelCount() * m_audioFormat.sampleSize() / 8);
        mediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, alignmentBlock);
        UINT32 avgBytesPerSec = UINT32(m_audioFormat.sampleRate() * m_audioFormat.sampleSize() / 8 * m_audioFormat.channelCount());
        mediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avgBytesPerSec);
        mediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, UINT32(m_audioFormat.sampleSize()));
        mediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

        if (m_mfOutputType)
            m_mfOutputType->Release();
        m_mfOutputType = mediaType;
    } else {
        if (m_mfOutputType)
            m_mfOutputType->Release();
        m_mfOutputType = NULL;
    }

    if (m_sourceReady && m_state == QAudioDecoder::StoppedState) {
        updateResamplerOutputType();
    } else {
        m_resamplerDirty = true;
    }

    emit formatChanged(m_audioFormat);
}

QAudioBuffer MFAudioDecoderControl::read()
{
    if (!m_bufferReady)
        return QAudioBuffer();
    QAudioBuffer buffer = m_cachedAudioBuffer;
    m_bufferReady = false;
    emit bufferAvailableChanged(m_bufferReady);
    m_decoderSourceReader->readNextSample();
    return buffer;
}

bool MFAudioDecoderControl::bufferAvailable() const
{
    return m_bufferReady;
}

qint64 MFAudioDecoderControl::position() const
{
    return m_position;
}

qint64 MFAudioDecoderControl::duration() const
{
    return m_duration;
}
