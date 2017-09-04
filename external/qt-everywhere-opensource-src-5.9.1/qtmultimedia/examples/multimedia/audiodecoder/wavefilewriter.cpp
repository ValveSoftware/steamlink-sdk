/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "wavefilewriter.h"

struct chunk
{
    char        id[4];
    quint32     size;
};

struct RIFFHeader
{
    chunk       descriptor;     // "RIFF"
    char        type[4];        // "WAVE"
};

struct WAVEHeader
{
    chunk       descriptor;
    quint16     audioFormat;
    quint16     numChannels;
    quint32     sampleRate;
    quint32     byteRate;
    quint16     blockAlign;
    quint16     bitsPerSample;
};

struct DATAHeader
{
    chunk       descriptor;
};

struct CombinedHeader
{
    RIFFHeader  riff;
    WAVEHeader  wave;
    DATAHeader  data;
};
static const int HeaderLength = sizeof(CombinedHeader);


WaveFileWriter::WaveFileWriter(QObject *parent)
    : QObject(parent)
    , m_dataLength(0)
{
}

WaveFileWriter::~WaveFileWriter()
{
    close();
}

bool WaveFileWriter::open(const QString& fileName, const QAudioFormat& format)
{
    if (file.isOpen())
        return false; // file already open

    if (format.codec() != "audio/pcm" || format.sampleType() != QAudioFormat::SignedInt)
        return false; // data format is not supported

    file.setFileName(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return false; // unable to open file for writing

    if (!writeHeader(format))
        return false;

    m_format = format;
    return true;
}

bool WaveFileWriter::write(const QAudioBuffer &buffer)
{
    if (buffer.format() != m_format)
        return false; // buffer format has changed

    qint64 written = file.write((const char *)buffer.constData(), buffer.byteCount());
    m_dataLength += written;
    return written == buffer.byteCount();
}

bool WaveFileWriter::close()
{
    bool result = false;
    if (file.isOpen()) {
        Q_ASSERT(m_dataLength < INT_MAX);
        result = writeDataLength();

        m_dataLength = 0;
        file.close();
    }
    return result;
}

bool WaveFileWriter::writeHeader(const QAudioFormat &format)
{
    // check if format is supported
    if (format.byteOrder() == QAudioFormat::BigEndian || format.sampleType() != QAudioFormat::SignedInt)
        return false;

    CombinedHeader header;
    memset(&header, 0, HeaderLength);

#ifndef Q_LITTLE_ENDIAN
    // only implemented for LITTLE ENDIAN
    return false;
#else
    // RIFF header
    memcpy(header.riff.descriptor.id, "RIFF", 4);
    header.riff.descriptor.size = 0; // this will be updated with correct duration:
                                     // m_dataLength + HeaderLength - 8
    // WAVE header
    memcpy(header.riff.type, "WAVE", 4);
    memcpy(header.wave.descriptor.id, "fmt ", 4);
    header.wave.descriptor.size = quint32(16);
    header.wave.audioFormat = quint16(1);
    header.wave.numChannels = quint16(format.channelCount());
    header.wave.sampleRate = quint32(format.sampleRate());
    header.wave.byteRate = quint32(format.sampleRate() * format.channelCount() * format.sampleSize() / 8);
    header.wave.blockAlign = quint16(format.channelCount() * format.sampleSize() / 8);
    header.wave.bitsPerSample = quint16(format.sampleSize());

    // DATA header
    memcpy(header.data.descriptor.id,"data", 4);
    header.data.descriptor.size = 0; // this will be updated with correct data length: m_dataLength

    return (file.write(reinterpret_cast<const char *>(&header), HeaderLength) == HeaderLength);
#endif
}

bool WaveFileWriter::writeDataLength()
{
#ifndef Q_LITTLE_ENDIAN
    // only implemented for LITTLE ENDIAN
    return false;
#endif

    if (file.isSequential())
        return false;

    // seek to RIFF header size, see header.riff.descriptor.size above
    if (!file.seek(4))
        return false;

    quint32 length = m_dataLength + HeaderLength - 8;
    if (file.write(reinterpret_cast<const char *>(&length), 4) != 4)
        return false;

    // seek to DATA header size, see header.data.descriptor.size above
    if (!file.seek(40))
        return false;

    return file.write(reinterpret_cast<const char *>(&m_dataLength), 4) == 4;
}
