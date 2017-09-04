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

#include "qgstreameraudioencode.h"
#include "qgstreamercapturesession.h"
#include "qgstreamermediacontainercontrol.h"
#include <private/qgstutils_p.h>

#include <QtCore/qdebug.h>

#include <math.h>

QGstreamerAudioEncode::QGstreamerAudioEncode(QObject *parent)
    :QAudioEncoderSettingsControl(parent)
    , m_codecs(QGstCodecsInfo::AudioEncoder)
{
    for (const QString& codecName : m_codecs.supportedCodecs()) {
        GstElementFactory *factory = gst_element_factory_find(m_codecs.codecElement(codecName).constData());

        if (factory) {
            m_streamTypes.insert(codecName,
                                 QGstreamerMediaContainerControl::supportedStreamTypes(factory, GST_PAD_SRC));

            gst_object_unref(GST_OBJECT(factory));
        }
    }
}

QGstreamerAudioEncode::~QGstreamerAudioEncode()
{
}

QStringList QGstreamerAudioEncode::supportedAudioCodecs() const
{
    return m_codecs.supportedCodecs();
}

QString QGstreamerAudioEncode::codecDescription(const QString &codecName) const
{
    return m_codecs.codecDescription(codecName);
}

QStringList QGstreamerAudioEncode::supportedEncodingOptions(const QString &codec) const
{
    return m_codecs.codecOptions(codec);
}

QVariant QGstreamerAudioEncode::encodingOption(
        const QString &codec, const QString &name) const
{
    return m_options[codec].value(name);
}

void QGstreamerAudioEncode::setEncodingOption(
        const QString &codec, const QString &name, const QVariant &value)
{
    m_options[codec][name] = value;
}

QList<int> QGstreamerAudioEncode::supportedSampleRates(const QAudioEncoderSettings &, bool *) const
{
    //TODO check element caps to find actual values

    return QList<int>();
}

QAudioEncoderSettings QGstreamerAudioEncode::audioSettings() const
{
    return m_audioSettings;
}

void QGstreamerAudioEncode::setAudioSettings(const QAudioEncoderSettings &settings)
{
    m_audioSettings = settings;
}


GstElement *QGstreamerAudioEncode::createEncoder()
{
    QString codec = m_audioSettings.codec();
    GstElement *encoderElement = gst_element_factory_make(m_codecs.codecElement(codec).constData(), NULL);
    if (!encoderElement)
        return 0;

    GstBin * encoderBin = GST_BIN(gst_bin_new("audio-encoder-bin"));

    GstElement *sinkCapsFilter = gst_element_factory_make("capsfilter", NULL);
    GstElement *srcCapsFilter = gst_element_factory_make("capsfilter", NULL);

    gst_bin_add_many(encoderBin, sinkCapsFilter, encoderElement, srcCapsFilter, NULL);
    gst_element_link_many(sinkCapsFilter, encoderElement, srcCapsFilter, NULL);

    // add ghostpads
    GstPad *pad = gst_element_get_static_pad(sinkCapsFilter, "sink");
    gst_element_add_pad(GST_ELEMENT(encoderBin), gst_ghost_pad_new("sink", pad));
    gst_object_unref(GST_OBJECT(pad));

    pad = gst_element_get_static_pad(srcCapsFilter, "src");
    gst_element_add_pad(GST_ELEMENT(encoderBin), gst_ghost_pad_new("src", pad));
    gst_object_unref(GST_OBJECT(pad));

    if (m_audioSettings.sampleRate() > 0 || m_audioSettings.channelCount() > 0) {
        GstCaps *caps = gst_caps_new_empty();
        GstStructure *structure = qt_gst_structure_new_empty(QT_GSTREAMER_RAW_AUDIO_MIME);

        if (m_audioSettings.sampleRate() > 0)
            gst_structure_set(structure, "rate", G_TYPE_INT, m_audioSettings.sampleRate(), NULL );

        if (m_audioSettings.channelCount() > 0)
            gst_structure_set(structure, "channels", G_TYPE_INT, m_audioSettings.channelCount(), NULL );

        gst_caps_append_structure(caps,structure);

        g_object_set(G_OBJECT(sinkCapsFilter), "caps", caps, NULL);

        gst_caps_unref(caps);
    }

    // Some encoders support several codecs. Setting a caps filter downstream with the desired
    // codec (which is actually a string representation of the caps) will make sure we use the
    // correct codec.
    GstCaps *caps = gst_caps_from_string(codec.toUtf8().constData());
    g_object_set(G_OBJECT(srcCapsFilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    if (encoderElement) {
        if (m_audioSettings.encodingMode() == QMultimedia::ConstantQualityEncoding) {
            QMultimedia::EncodingQuality qualityValue = m_audioSettings.quality();

            if (codec == QLatin1String("audio/x-vorbis")) {
                double qualityTable[] = {
                    0.1, //VeryLow
                    0.3, //Low
                    0.5, //Normal
                    0.7, //High
                    1.0 //VeryHigh
                };
                g_object_set(G_OBJECT(encoderElement), "quality", qualityTable[qualityValue], NULL);
            } else if (codec == QLatin1String("audio/mpeg")) {
                g_object_set(G_OBJECT(encoderElement), "target", 0, NULL); //constant quality mode
                qreal quality[] = {
                    1, //VeryLow
                    3, //Low
                    5, //Normal
                    7, //High
                    9 //VeryHigh
                };
                g_object_set(G_OBJECT(encoderElement), "quality", quality[qualityValue], NULL);
            } else if (codec == QLatin1String("audio/x-speex")) {
                //0-10 range with default 8
                double qualityTable[] = {
                    2, //VeryLow
                    5, //Low
                    8, //Normal
                    9, //High
                    10 //VeryHigh
                };
                g_object_set(G_OBJECT(encoderElement), "quality", qualityTable[qualityValue], NULL);
            } else if (codec.startsWith("audio/AMR")) {
                int band[] = {
                    0, //VeryLow
                    2, //Low
                    4, //Normal
                    6, //High
                    7  //VeryHigh
                };

                g_object_set(G_OBJECT(encoderElement), "band-mode", band[qualityValue], NULL);
            }
        } else {
            int bitrate = m_audioSettings.bitRate();
            if (bitrate > 0) {
                if (codec == QLatin1String("audio/mpeg")) {
                    g_object_set(G_OBJECT(encoderElement), "target", 1, NULL); //constant bitrate mode
                }
                g_object_set(G_OBJECT(encoderElement), "bitrate", bitrate, NULL);
            }
        }

        QMap<QString, QVariant> options = m_options.value(codec);
        QMapIterator<QString,QVariant> it(options);
        while (it.hasNext()) {
            it.next();
            QString option = it.key();
            QVariant value = it.value();

            switch (value.type()) {
            case QVariant::Int:
                g_object_set(G_OBJECT(encoderElement), option.toLatin1(), value.toInt(), NULL);
                break;
            case QVariant::Bool:
                g_object_set(G_OBJECT(encoderElement), option.toLatin1(), value.toBool(), NULL);
                break;
            case QVariant::Double:
                g_object_set(G_OBJECT(encoderElement), option.toLatin1(), value.toDouble(), NULL);
                break;
            case QVariant::String:
                g_object_set(G_OBJECT(encoderElement), option.toLatin1(), value.toString().toUtf8().constData(), NULL);
                break;
            default:
                qWarning() << "unsupported option type:" << option << value;
                break;
            }

        }
    }

    return GST_ELEMENT(encoderBin);
}


QSet<QString> QGstreamerAudioEncode::supportedStreamTypes(const QString &codecName) const
{
    return m_streamTypes.value(codecName);
}
