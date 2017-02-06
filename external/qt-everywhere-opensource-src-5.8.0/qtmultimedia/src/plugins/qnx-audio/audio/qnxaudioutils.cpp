/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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

#include "qnxaudioutils.h"

QT_BEGIN_NAMESPACE

snd_pcm_channel_params_t QnxAudioUtils::formatToChannelParams(const QAudioFormat &format, QAudio::Mode mode, int fragmentSize)
{
    snd_pcm_channel_params_t params;
    memset(&params, 0, sizeof(params));
    params.channel = (mode == QAudio::AudioOutput) ? SND_PCM_CHANNEL_PLAYBACK : SND_PCM_CHANNEL_CAPTURE;
    params.mode = SND_PCM_MODE_BLOCK;
    params.start_mode = SND_PCM_START_DATA;
    params.stop_mode = SND_PCM_STOP_ROLLOVER;
    params.buf.block.frag_size = fragmentSize;
    params.buf.block.frags_min = 1;
    params.buf.block.frags_max = 1;
    strcpy(params.sw_mixer_subchn_name, "QAudio Channel");

    params.format.interleave = 1;
    params.format.rate = format.sampleRate();
    params.format.voices = format.channelCount();

    switch (format.sampleSize()) {
    case 8:
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            params.format.format = SND_PCM_SFMT_S8;
            break;
        case QAudioFormat::UnSignedInt:
            params.format.format = SND_PCM_SFMT_U8;
            break;
        default:
            break;
        }
        break;

    case 16:
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                params.format.format = SND_PCM_SFMT_S16_LE;
            } else {
                params.format.format = SND_PCM_SFMT_S16_BE;
            }
            break;
        case QAudioFormat::UnSignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                params.format.format = SND_PCM_SFMT_U16_LE;
            } else {
                params.format.format = SND_PCM_SFMT_U16_BE;
            }
            break;
        default:
            break;
        }
        break;

    case 32:
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                params.format.format = SND_PCM_SFMT_S32_LE;
            } else {
                params.format.format = SND_PCM_SFMT_S32_BE;
            }
            break;
        case QAudioFormat::UnSignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                params.format.format = SND_PCM_SFMT_U32_LE;
            } else {
                params.format.format = SND_PCM_SFMT_U32_BE;
            }
            break;
        case QAudioFormat::Float:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                params.format.format = SND_PCM_SFMT_FLOAT_LE;
            } else {
                params.format.format = SND_PCM_SFMT_FLOAT_BE;
            }
            break;
        default:
            break;
        }
        break;
    }

    return params;
}

QT_END_NAMESPACE
