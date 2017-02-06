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

#ifdef QMEDIA_GSTREAMER_CAMERABIN
#include <gst/pbutils/pbutils.h>
#include <gst/pbutils/encoding-profile.h>
#endif


QGstCodecsInfo::QGstCodecsInfo(QGstCodecsInfo::ElementType elementType)
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

    GstCaps *allCaps = supportedElementCaps(gstElementType);
    GstCaps *caps = gst_caps_new_empty();

    uint codecsCount = gst_caps_get_size(allCaps);
    for (uint i=0; i<codecsCount; i++) {
        gst_caps_append_structure(caps, gst_caps_steal_structure(allCaps, 0));
        gchar * capsString = gst_caps_to_string(caps);

        QString codec = QLatin1String(capsString);
        m_codecs.append(codec);

#ifdef QMEDIA_GSTREAMER_CAMERABIN
        gchar *description = gst_pb_utils_get_codec_description(caps);
        m_codecDescriptions.insert(codec, QString::fromUtf8(description));

        if (description)
            g_free(description);
#else
        m_codecDescriptions.insert(codec, codec);
#endif

        if (capsString)
            g_free(capsString);

        gst_caps_remove_structure(caps, 0);
    }

    gst_caps_unref(caps);
    gst_caps_unref(allCaps);
#else
    Q_UNUSED(elementType);
#endif // GST_CHECK_VERSION(0,10,31)
}

QStringList QGstCodecsInfo::supportedCodecs() const
{
    return m_codecs;
}

QString QGstCodecsInfo::codecDescription(const QString &codec) const
{
    return m_codecDescriptions.value(codec);
}

#if GST_CHECK_VERSION(0,10,31)

/*!
  List all supported caps for all installed elements of type \a elementType.

  Caps are simplified to mime type and a few field necessary to distinguish
  different codecs like mpegversion or layer.
 */
GstCaps* QGstCodecsInfo::supportedElementCaps(GstElementFactoryListType elementType,
                                         GstRank minimumRank,
                                         GstPadDirection padDirection)
{
    GList *elements = gst_element_factory_list_get_elements(elementType, minimumRank);
    GstCaps *res = gst_caps_new_empty();

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

            if (padTemplate->direction == padDirection) {
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

#if GST_CHECK_VERSION(1,0,0)
                    res =
#endif
                    gst_caps_merge_structure(res, newStructure);

                }
                gst_caps_unref(caps);
            }
        }
    }
    gst_plugin_feature_list_free(elements);

    return res;
}
#endif //GST_CHECK_VERSION(0,10,31)
