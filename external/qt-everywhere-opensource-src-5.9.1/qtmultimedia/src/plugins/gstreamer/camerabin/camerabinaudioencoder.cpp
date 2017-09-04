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

#include <QtMultimedia/private/qtmultimediaglobal_p.h>
#include "camerabinaudioencoder.h"
#include "camerabincontainer.h"
#include <private/qgstutils_p.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

CameraBinAudioEncoder::CameraBinAudioEncoder(QObject *parent)
    :QAudioEncoderSettingsControl(parent)
#if QT_CONFIG(gstreamer_encodingprofiles)
    , m_codecs(QGstCodecsInfo::AudioEncoder)
#endif
{
}

CameraBinAudioEncoder::~CameraBinAudioEncoder()
{
}

QStringList CameraBinAudioEncoder::supportedAudioCodecs() const
{
#if QT_CONFIG(gstreamer_encodingprofiles)
    return m_codecs.supportedCodecs();
#else
    return QStringList();
#endif
}

QString CameraBinAudioEncoder::codecDescription(const QString &codecName) const
{
#if QT_CONFIG(gstreamer_encodingprofiles)
    return m_codecs.codecDescription(codecName);
#else
    Q_UNUSED(codecName)
    return QString();
#endif
}

QList<int> CameraBinAudioEncoder::supportedSampleRates(const QAudioEncoderSettings &, bool *) const
{
    //TODO check element caps to find actual values

    return QList<int>();
}

QAudioEncoderSettings CameraBinAudioEncoder::audioSettings() const
{
    return m_audioSettings;
}

void CameraBinAudioEncoder::setAudioSettings(const QAudioEncoderSettings &settings)
{
    if (m_audioSettings != settings) {
        m_audioSettings = settings;
        m_actualAudioSettings = settings;
        emit settingsChanged();
    }
}

QAudioEncoderSettings CameraBinAudioEncoder::actualAudioSettings() const
{
    return m_actualAudioSettings;
}

void CameraBinAudioEncoder::setActualAudioSettings(const QAudioEncoderSettings &settings)
{
    m_actualAudioSettings = settings;
}

void CameraBinAudioEncoder::resetActualSettings()
{
    m_actualAudioSettings = m_audioSettings;
}

#if QT_CONFIG(gstreamer_encodingprofiles)

GstEncodingProfile *CameraBinAudioEncoder::createProfile()
{
    QString codec = m_actualAudioSettings.codec();
    QString preset = m_actualAudioSettings.encodingOption(QStringLiteral("preset")).toString();
    GstCaps *caps;

    if (codec.isEmpty())
        return 0;
    else
        caps = gst_caps_from_string(codec.toLatin1());

    GstEncodingProfile *profile = (GstEncodingProfile *)gst_encoding_audio_profile_new(
                caps,
                !preset.isEmpty() ? preset.toLatin1().constData() : NULL, //preset
                NULL,   //restriction
                0);     //presence

    gst_caps_unref(caps);

    return profile;
}

#endif

void CameraBinAudioEncoder::applySettings(GstElement *encoder)
{
    GObjectClass * const objectClass = G_OBJECT_GET_CLASS(encoder);
    const char * const name = qt_gst_element_get_factory_name(encoder);

    const bool isVorbis = qstrcmp(name, "vorbisenc") == 0;

    const int bitRate = m_actualAudioSettings.bitRate();
    if (!isVorbis && bitRate == -1) {
        // Bit rate is invalid, don't evaluate the remaining conditions unless the encoder is
        // vorbisenc which is known to accept -1 as an unspecified bitrate.
    } else if (g_object_class_find_property(objectClass, "bitrate")) {
        g_object_set(G_OBJECT(encoder), "bitrate", bitRate, NULL);
    } else if (g_object_class_find_property(objectClass, "target-bitrate")) {
        g_object_set(G_OBJECT(encoder), "target-bitrate", bitRate, NULL);
    }

    if (isVorbis) {
        static const double qualities[] = { 0.1, 0.3, 0.5, 0.7, 1.0 };
        g_object_set(G_OBJECT(encoder), "quality", qualities[m_actualAudioSettings.quality()], NULL);
    }
}

QT_END_NAMESPACE
