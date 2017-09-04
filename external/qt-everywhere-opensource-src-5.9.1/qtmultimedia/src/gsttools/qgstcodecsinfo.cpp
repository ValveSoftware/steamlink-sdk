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

#include "qgstcodecsinfo_p.h"
#include "qgstutils_p.h"
#include <QtCore/qset.h>

#include <gst/pbutils/pbutils.h>

QGstCodecsInfo::QGstCodecsInfo(QGstCodecsInfo::ElementType elementType)
{
    updateCodecs(elementType);
}

QStringList QGstCodecsInfo::supportedCodecs() const
{
    return m_codecs;
}

QString QGstCodecsInfo::codecDescription(const QString &codec) const
{
    return m_codecInfo.value(codec).description;
}

QByteArray QGstCodecsInfo::codecElement(const QString &codec) const

{
    return m_codecInfo.value(codec).elementName;
}

QStringList QGstCodecsInfo::codecOptions(const QString &codec) const
{
    QStringList options;

    QByteArray elementName = m_codecInfo.value(codec).elementName;
    if (elementName.isEmpty())
        return options;

    GstElement *element = gst_element_factory_make(elementName, NULL);
    if (element) {
        guint numProperties;
        GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(element),
                                                                 &numProperties);
        for (guint j = 0; j < numProperties; ++j) {
            GParamSpec *property = properties[j];
            // ignore some properties
            if (strcmp(property->name, "name") == 0 || strcmp(property->name, "parent") == 0)
                continue;

            options.append(QLatin1String(property->name));
        }
        g_free(properties);
        gst_object_unref(element);
    }

    return options;
}

void QGstCodecsInfo::updateCodecs(ElementType elementType)
{
    m_codecs.clear();
    m_codecInfo.clear();

    GList *elements = elementFactories(elementType);

    QSet<QByteArray> fakeEncoderMimeTypes;
    fakeEncoderMimeTypes << "unknown/unknown"
                  << "audio/x-raw-int" << "audio/x-raw-float"
                  << "video/x-raw-yuv" << "video/x-raw-rgb";

    QSet<QByteArray> fieldsToAdd;
    fieldsToAdd << "mpegversion" << "layer" << "layout" << "raversion"
                << "wmaversion" << "wmvversion" << "variant";

    GList *element = elements;
    while (element) {
        GstElementFactory *factory = (GstElementFactory *)element->data;
        element = element->next;

        const GList *padTemplates = gst_element_factory_get_static_pad_templates(factory);
        while (padTemplates) {
            GstStaticPadTemplate *padTemplate = (GstStaticPadTemplate *)padTemplates->data;
            padTemplates = padTemplates->next;

            if (padTemplate->direction == GST_PAD_SRC) {
                GstCaps *caps = gst_static_caps_get(&padTemplate->static_caps);
                for (uint i=0; i<gst_caps_get_size(caps); i++) {
                    const GstStructure *structure = gst_caps_get_structure(caps, i);

                    //skip "fake" encoders
                    if (fakeEncoderMimeTypes.contains(gst_structure_get_name(structure)))
                        continue;

                    GstStructure *newStructure = qt_gst_structure_new_empty(gst_structure_get_name(structure));

                    //add structure fields to distinguish between formats with similar mime types,
                    //like audio/mpeg
                    for (int j=0; j<gst_structure_n_fields(structure); j++) {
                        const gchar* fieldName = gst_structure_nth_field_name(structure, j);
                        if (fieldsToAdd.contains(fieldName)) {
                            const GValue *value = gst_structure_get_value(structure, fieldName);
                            GType valueType = G_VALUE_TYPE(value);

                            //don't add values of range type,
                            //gst_pb_utils_get_codec_description complains about not fixed caps

                            if (valueType != GST_TYPE_INT_RANGE && valueType != GST_TYPE_DOUBLE_RANGE &&
                                valueType != GST_TYPE_FRACTION_RANGE && valueType != GST_TYPE_LIST &&
                                valueType != GST_TYPE_ARRAY)
                                gst_structure_set_value(newStructure, fieldName, value);
                        }
                    }

                    GstCaps *newCaps = gst_caps_new_full(newStructure, NULL);

                    gchar *capsString = gst_caps_to_string(newCaps);
                    QString codec = QLatin1String(capsString);
                    if (capsString)
                        g_free(capsString);
                    GstRank rank = GstRank(gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE(factory)));

                    // If two elements provide the same codec, use the highest ranked one
                    QMap<QString, CodecInfo>::const_iterator it = m_codecInfo.find(codec);
                    if (it == m_codecInfo.constEnd() || it->rank < rank) {
                        if (it == m_codecInfo.constEnd())
                            m_codecs.append(codec);

                        CodecInfo info;
                        info.elementName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

                        gchar *description = gst_pb_utils_get_codec_description(newCaps);
                        info.description = QString::fromUtf8(description);
                        if (description)
                            g_free(description);

                        info.rank = rank;

                        m_codecInfo.insert(codec, info);
                    }

                    gst_caps_unref(newCaps);
                }
                gst_caps_unref(caps);
            }
        }
    }

    gst_plugin_feature_list_free(elements);
}

#if !GST_CHECK_VERSION(0, 10, 31)
static gboolean element_filter(GstPluginFeature *feature, gpointer user_data)
{
    if (Q_UNLIKELY(!GST_IS_ELEMENT_FACTORY(feature)))
        return FALSE;

    const QGstCodecsInfo::ElementType type = *reinterpret_cast<QGstCodecsInfo::ElementType *>(user_data);

    const gchar *klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(feature));
    if (type == QGstCodecsInfo::AudioEncoder && !(strstr(klass, "Encoder") && strstr(klass, "Audio")))
        return FALSE;
    if (type == QGstCodecsInfo::VideoEncoder && !(strstr(klass, "Encoder") && strstr(klass, "Video")))
        return FALSE;
    if (type == QGstCodecsInfo::Muxer && !strstr(klass, "Muxer"))
        return FALSE;

    guint rank = gst_plugin_feature_get_rank(feature);
    if (rank < GST_RANK_MARGINAL)
        return FALSE;

    return TRUE;
}

static gint compare_plugin_func(const void *item1, const void *item2)
{
    GstPluginFeature *f1 = reinterpret_cast<GstPluginFeature *>(const_cast<void *>(item1));
    GstPluginFeature *f2 = reinterpret_cast<GstPluginFeature *>(const_cast<void *>(item2));

    gint diff = gst_plugin_feature_get_rank(f2) - gst_plugin_feature_get_rank(f1);
    if (diff != 0)
        return diff;

    return strcmp(gst_plugin_feature_get_name(f1), gst_plugin_feature_get_name (f2));
}
#endif

GList *QGstCodecsInfo::elementFactories(ElementType elementType) const
{
#if GST_CHECK_VERSION(0,10,31)
    GstElementFactoryListType gstElementType = 0;
    switch (elementType) {
    case AudioEncoder:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER;
        break;
    case VideoEncoder:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER;
        break;
    case Muxer:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_MUXER;
        break;
    }

    return gst_element_factory_list_get_elements(gstElementType, GST_RANK_MARGINAL);
#else
    GList *result = gst_registry_feature_filter(gst_registry_get_default(),
                                                element_filter,
                                                FALSE, &elementType);
    result = g_list_sort(result, compare_plugin_func);
    return result;
#endif
}
