/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/qendian.h>
#include "wavheader.h"


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


WavHeader::WavHeader(const QAudioFormat &format, qint64 dataLength)
    :   m_format(format)
    ,   m_dataLength(dataLength)
{

}

bool WavHeader::read(QIODevice &device)
{
    bool result = true;

    if (!device.isSequential())
        result = device.seek(0);
    // else, assume that current position is the start of the header

    if (result) {
        CombinedHeader header;
        result = (device.read(reinterpret_cast<char *>(&header), HeaderLength) == HeaderLength);
        if (result) {
            if ((memcmp(&header.riff.descriptor.id, "RIFF", 4) == 0
                || memcmp(&header.riff.descriptor.id, "RIFX", 4) == 0)
                && memcmp(&header.riff.type, "WAVE", 4) == 0
                && memcmp(&header.wave.descriptor.id, "fmt ", 4) == 0
                && header.wave.audioFormat == 1 // PCM
            ) {
                if (memcmp(&header.riff.descriptor.id, "RIFF", 4) == 0)
                    m_format.setByteOrder(QAudioFormat::LittleEndian);
                else
                    m_format.setByteOrder(QAudioFormat::BigEndian);

                m_format.setChannelCount(qFromLittleEndian<quint16>(header.wave.numChannels));
                m_format.setCodec("audio/pcm");
                m_format.setSampleRate(qFromLittleEndian<quint32>(header.wave.sampleRate));
                m_format.setSampleSize(qFromLittleEndian<quint16>(header.wave.bitsPerSample));

                switch(header.wave.bitsPerSample) {
                case 8:
                    m_format.setSampleType(QAudioFormat::UnSignedInt);
                    break;
                case 16:
                    m_format.setSampleType(QAudioFormat::SignedInt);
                    break;
                default:
                    result = false;
                }

                m_dataLength = device.size() - HeaderLength;
            } else {
                result = false;
            }
        }
    }

    return result;
}

bool WavHeader::write(QIODevice &device)
{
    CombinedHeader header;

    memset(&header, 0, HeaderLength);

    // RIFF header
    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
        memcpy(header.riff.descriptor.id,"RIFF",4);
    else
        memcpy(header.riff.descriptor.id,"RIFX",4);
    qToLittleEndian<quint32>(quint32(m_dataLength + HeaderLength - 8),
                             reinterpret_cast<unsigned char*>(&header.riff.descriptor.size));
    memcpy(header.riff.type, "WAVE",4);

    // WAVE header
    memcpy(header.wave.descriptor.id,"fmt ",4);
    qToLittleEndian<quint32>(quint32(16),
                             reinterpret_cast<unsigned char*>(&header.wave.descriptor.size));
    qToLittleEndian<quint16>(quint16(1),
                             reinterpret_cast<unsigned char*>(&header.wave.audioFormat));
    qToLittleEndian<quint16>(quint16(m_format.channelCount()),
                             reinterpret_cast<unsigned char*>(&header.wave.numChannels));
    qToLittleEndian<quint32>(quint32(m_format.sampleRate()),
                             reinterpret_cast<unsigned char*>(&header.wave.sampleRate));
    qToLittleEndian<quint32>(quint32(m_format.sampleRate() * m_format.channelCount() * m_format.sampleSize() / 8),
                             reinterpret_cast<unsigned char*>(&header.wave.byteRate));
    qToLittleEndian<quint16>(quint16(m_format.channelCount() * m_format.sampleSize() / 8),
                             reinterpret_cast<unsigned char*>(&header.wave.blockAlign));
    qToLittleEndian<quint16>(quint16(m_format.sampleSize()),
                             reinterpret_cast<unsigned char*>(&header.wave.bitsPerSample));

    // DATA header
    memcpy(header.data.descriptor.id,"data",4);
    qToLittleEndian<quint32>(quint32(m_dataLength),
                             reinterpret_cast<unsigned char*>(&header.data.descriptor.size));

    return (device.write(reinterpret_cast<const char *>(&header), HeaderLength) == HeaderLength);
}

const QAudioFormat& WavHeader::format() const
{
    return m_format;
}

qint64 WavHeader::dataLength() const
{
    return m_dataLength;
}

qint64 WavHeader::headerLength()
{
    return HeaderLength;
}

bool WavHeader::writeDataLength(QIODevice &device, qint64 dataLength)
{
    bool result = false;
    if (!device.isSequential()) {
        device.seek(40);
        unsigned char dataLengthLE[4];
        qToLittleEndian<quint32>(quint32(dataLength), dataLengthLE);
        result = (device.write(reinterpret_cast<const char *>(dataLengthLE), 4) == 4);
    }
    return result;
}
