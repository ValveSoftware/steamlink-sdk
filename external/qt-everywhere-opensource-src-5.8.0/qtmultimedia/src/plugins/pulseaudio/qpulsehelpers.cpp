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

#include "qpulsehelpers.h"

QT_BEGIN_NAMESPACE

namespace QPulseAudioInternal
{
pa_sample_spec audioFormatToSampleSpec(const QAudioFormat &format)
{
    pa_sample_spec  spec;

    spec.rate = format.sampleRate();
    spec.channels = format.channelCount();

    if (format.sampleSize() == 8) {
        spec.format = PA_SAMPLE_U8;
    } else if (format.sampleSize() == 16) {
        switch (format.byteOrder()) {
        case QAudioFormat::BigEndian: spec.format = PA_SAMPLE_S16BE; break;
        case QAudioFormat::LittleEndian: spec.format = PA_SAMPLE_S16LE; break;
        }
    } else if (format.sampleSize() == 24) {
        switch (format.byteOrder()) {
        case QAudioFormat::BigEndian: spec.format = PA_SAMPLE_S24BE; break;
        case QAudioFormat::LittleEndian: spec.format = PA_SAMPLE_S24LE; break;
        }
    } else if (format.sampleSize() == 32) {
        switch (format.byteOrder()) {
        case QAudioFormat::BigEndian:
            format.sampleType() == QAudioFormat::Float ? spec.format = PA_SAMPLE_FLOAT32BE : spec.format = PA_SAMPLE_S32BE;
            break;
        case QAudioFormat::LittleEndian:
            format.sampleType() == QAudioFormat::Float ? spec.format = PA_SAMPLE_FLOAT32LE : spec.format = PA_SAMPLE_S32LE;
            break;
        }
    } else {
        spec.format = PA_SAMPLE_INVALID;
    }

    return spec;
}

#ifdef DEBUG_PULSE
QString stateToQString(pa_stream_state_t state)
{
    switch (state)
    {
    case PA_STREAM_UNCONNECTED: return "Unconnected";
    case PA_STREAM_CREATING:    return "Creating";
    case PA_STREAM_READY:       return "Ready";
    case PA_STREAM_FAILED:      return "Failed";
    case PA_STREAM_TERMINATED:  return "Terminated";
    }

    return QString("Unknown state: %0").arg(state);
}

QString sampleFormatToQString(pa_sample_format format)
{
    switch (format)
    {
    case PA_SAMPLE_U8:          return "Unsigned 8 Bit PCM.";
    case PA_SAMPLE_ALAW:        return "8 Bit a-Law ";
    case PA_SAMPLE_ULAW:        return "8 Bit mu-Law";
    case PA_SAMPLE_S16LE:       return "Signed 16 Bit PCM, little endian (PC).";
    case PA_SAMPLE_S16BE:       return "Signed 16 Bit PCM, big endian.";
    case PA_SAMPLE_FLOAT32LE:   return "32 Bit IEEE floating point, little endian (PC), range -1.0 to 1.0";
    case PA_SAMPLE_FLOAT32BE:   return "32 Bit IEEE floating point, big endian, range -1.0 to 1.0";
    case PA_SAMPLE_S32LE:       return "Signed 32 Bit PCM, little endian (PC).";
    case PA_SAMPLE_S32BE:       return "Signed 32 Bit PCM, big endian.";
    case PA_SAMPLE_S24LE:       return "Signed 24 Bit PCM packed, little endian (PC).";
    case PA_SAMPLE_S24BE:       return "Signed 24 Bit PCM packed, big endian.";
    case PA_SAMPLE_S24_32LE:    return "Signed 24 Bit PCM in LSB of 32 Bit words, little endian (PC).";
    case PA_SAMPLE_S24_32BE:    return "Signed 24 Bit PCM in LSB of 32 Bit words, big endian.";
    case PA_SAMPLE_MAX:         return "Upper limit of valid sample types.";
    case PA_SAMPLE_INVALID:     return "Invalid sample format";
    }

    return QString("Invalid value: %0").arg(format);
}

QString stateToQString(pa_context_state_t state)
{
    switch (state)
    {
    case PA_CONTEXT_UNCONNECTED:  return "Unconnected";
    case PA_CONTEXT_CONNECTING:   return "Connecting";
    case PA_CONTEXT_AUTHORIZING:  return "Authorizing";
    case PA_CONTEXT_SETTING_NAME: return "Setting Name";
    case PA_CONTEXT_READY:        return "Ready";
    case PA_CONTEXT_FAILED:       return "Failed";
    case PA_CONTEXT_TERMINATED:   return "Terminated";
    }

    return QString("Unknown state: %0").arg(state);
}
#endif

QAudioFormat sampleSpecToAudioFormat(pa_sample_spec spec)
{
    QAudioFormat format;
    format.setSampleRate(spec.rate);
    format.setChannelCount(spec.channels);
    format.setCodec("audio/pcm");

    switch (spec.format) {
        case PA_SAMPLE_U8:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::UnSignedInt);
            format.setSampleSize(8);
        break;
        case PA_SAMPLE_ALAW:
        // TODO:
        break;
        case PA_SAMPLE_ULAW:
        // TODO:
        break;
        case PA_SAMPLE_S16LE:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(16);
        break;
        case PA_SAMPLE_S16BE:
            format.setByteOrder(QAudioFormat::BigEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(16);
        break;
        case PA_SAMPLE_FLOAT32LE:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::Float);
            format.setSampleSize(32);
        break;
        case PA_SAMPLE_FLOAT32BE:
            format.setByteOrder(QAudioFormat::BigEndian);
            format.setSampleType(QAudioFormat::Float);
            format.setSampleSize(32);
        break;
        case PA_SAMPLE_S32LE:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(32);
        break;
        case PA_SAMPLE_S32BE:
            format.setByteOrder(QAudioFormat::BigEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(32);
        break;
        case PA_SAMPLE_S24LE:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(24);
        break;
        case PA_SAMPLE_S24BE:
            format.setByteOrder(QAudioFormat::BigEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(24);
        break;
        case PA_SAMPLE_S24_32LE:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(24);
        break;
        case PA_SAMPLE_S24_32BE:
            format.setByteOrder(QAudioFormat::BigEndian);
            format.setSampleType(QAudioFormat::SignedInt);
            format.setSampleSize(24);
        break;
        case PA_SAMPLE_MAX:
        case PA_SAMPLE_INVALID:
        default:
            format.setByteOrder(QAudioFormat::LittleEndian);
            format.setSampleType(QAudioFormat::Unknown);
            format.setSampleSize(0);
    }

    return format;
}
}

QT_END_NAMESPACE
