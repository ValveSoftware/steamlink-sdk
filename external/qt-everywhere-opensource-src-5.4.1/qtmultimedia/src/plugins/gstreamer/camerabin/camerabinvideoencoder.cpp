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

#include "camerabinvideoencoder.h"
#include "camerabinsession.h"
#include "camerabincontainer.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

CameraBinVideoEncoder::CameraBinVideoEncoder(CameraBinSession *session)
    :QVideoEncoderSettingsControl(session),
     m_session(session),
     m_codecs(QGstCodecsInfo::VideoEncoder)
{
}

CameraBinVideoEncoder::~CameraBinVideoEncoder()
{
}

QList<QSize> CameraBinVideoEncoder::supportedResolutions(const QVideoEncoderSettings &settings, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    QPair<int,int> rate = rateAsRational(settings.frameRate());

    //select the closest supported rational rate to settings.frameRate()

    return m_session->supportedResolutions(rate, continuous, QCamera::CaptureVideo);
}

QList< qreal > CameraBinVideoEncoder::supportedFrameRates(const QVideoEncoderSettings &settings, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    QList< qreal > res;
    QPair<int,int> rate;

    foreach(rate, m_session->supportedFrameRates(settings.resolution(), continuous)) {
        if (rate.second > 0)
            res << qreal(rate.first)/rate.second;
    }

    return res;
}

QStringList CameraBinVideoEncoder::supportedVideoCodecs() const
{
    return m_codecs.supportedCodecs();
}

QString CameraBinVideoEncoder::videoCodecDescription(const QString &codecName) const
{
    return m_codecs.codecDescription(codecName);
}

QVideoEncoderSettings CameraBinVideoEncoder::videoSettings() const
{
    return m_videoSettings;
}

void CameraBinVideoEncoder::setVideoSettings(const QVideoEncoderSettings &settings)
{
    if (m_videoSettings != settings) {
        m_actualVideoSettings = settings;
        m_videoSettings = settings;
        emit settingsChanged();
    }
}

QVideoEncoderSettings CameraBinVideoEncoder::actualVideoSettings() const
{
    return m_actualVideoSettings;
}

void CameraBinVideoEncoder::setActualVideoSettings(const QVideoEncoderSettings &settings)
{
    m_actualVideoSettings = settings;
}

void CameraBinVideoEncoder::resetActualSettings()
{
    m_actualVideoSettings = m_videoSettings;
}


QPair<int,int> CameraBinVideoEncoder::rateAsRational(qreal frameRate) const
{
    if (frameRate > 0.001) {
        //convert to rational number
        QList<int> denumCandidates;
        denumCandidates << 1 << 2 << 3 << 5 << 10 << 25 << 30 << 50 << 100 << 1001 << 1000;

        qreal error = 1.0;
        int num = 1;
        int denum = 1;

        foreach (int curDenum, denumCandidates) {
            int curNum = qRound(frameRate*curDenum);
            qreal curError = qAbs(qreal(curNum)/curDenum - frameRate);

            if (curError < error) {
                error = curError;
                num = curNum;
                denum = curDenum;
            }

            if (curError < 1e-8)
                break;
        }

        return QPair<int,int>(num,denum);
    }

    return QPair<int,int>();
}

GstEncodingProfile *CameraBinVideoEncoder::createProfile()
{
    QString codec = m_actualVideoSettings.codec();
    QString preset = m_actualVideoSettings.encodingOption(QStringLiteral("preset")).toString();

    GstCaps *caps;

    if (codec.isEmpty())
        caps = 0;
    else
        caps = gst_caps_from_string(codec.toLatin1());

    GstEncodingVideoProfile *profile = gst_encoding_video_profile_new(
                caps,
                !preset.isEmpty() ? preset.toLatin1().constData() : NULL, //preset
                NULL, //restriction
                1); //presence

    gst_caps_unref(caps);

    gst_encoding_video_profile_set_pass(profile, 0);
    gst_encoding_video_profile_set_variableframerate(profile, TRUE);

    return (GstEncodingProfile *)profile;
}

void CameraBinVideoEncoder::applySettings(GstElement *encoder)
{
    GObjectClass * const objectClass = G_OBJECT_GET_CLASS(encoder);
    const char * const name = gst_plugin_feature_get_name(
                GST_PLUGIN_FEATURE(gst_element_get_factory(encoder)));

    const int bitRate = m_actualVideoSettings.bitRate();
    if (bitRate == -1) {
        // Bit rate is invalid, don't evaluate the remaining conditions.
    } else if (g_object_class_find_property(objectClass, "bitrate")) {
        g_object_set(G_OBJECT(encoder), "bitrate", bitRate, NULL);
    } else if (g_object_class_find_property(objectClass, "target-bitrate")) {
        g_object_set(G_OBJECT(encoder), "target-bitrate", bitRate, NULL);
    }

    if (qstrcmp(name, "theoraenc") == 0) {
        static const int qualities[] = { 8, 16, 32, 45, 60 };
        g_object_set(G_OBJECT(encoder), "quality", qualities[m_actualVideoSettings.quality()], NULL);
    } else if (qstrncmp(name, "avenc_", 6) == 0) {
        if (g_object_class_find_property(objectClass, "pass")) {
            static const int modes[] = { 0, 2, 512, 1024 };
            g_object_set(G_OBJECT(encoder), "pass", modes[m_actualVideoSettings.encodingMode()], NULL);
        }
        if (g_object_class_find_property(objectClass, "quantizer")) {
            static const double qualities[] = { 20, 8.0, 3.0, 2.5, 2.0 };
            g_object_set(G_OBJECT(encoder), "quantizer", qualities[m_actualVideoSettings.quality()], NULL);
        }
    } else if (qstrncmp(name, "omx", 3) == 0) {
        if (!g_object_class_find_property(objectClass, "control-rate")) {
        } else switch (m_actualVideoSettings.encodingMode()) {
        case QMultimedia::ConstantBitRateEncoding:
            g_object_set(G_OBJECT(encoder), "control-rate", 2, NULL);
            break;
        case QMultimedia::AverageBitRateEncoding:
            g_object_set(G_OBJECT(encoder), "control-rate", 1, NULL);
            break;
        default:
            g_object_set(G_OBJECT(encoder), "control-rate", 0, NULL);
        }
    }
}

QT_END_NAMESPACE
