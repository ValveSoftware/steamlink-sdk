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

#include "qgstreamermediacontainercontrol.h"


#include <QtCore/qdebug.h>

QGstreamerMediaContainerControl::QGstreamerMediaContainerControl(QObject *parent)
    :QMediaContainerControl(parent)
{
    QList<QByteArray> formatCandidates;
    formatCandidates << "matroska" << "ogg" << "mp4" << "wav" << "quicktime" << "avi" << "3gpp";
    formatCandidates << "flv" << "amr" << "asf" << "dv" << "gif";
    formatCandidates << "mpeg" << "vob" << "mpegts" << "3g2" << "3gp";
    formatCandidates << "raw";

    m_elementNames["matroska"] = "matroskamux";
    m_elementNames["ogg"] = "oggmux";
    m_elementNames["mp4"] = "ffmux_mp4";
    m_elementNames["quicktime"] = "ffmux_mov";
    m_elementNames["avi"] = "avimux";
    m_elementNames["3gpp"] = "gppmux";
    m_elementNames["flv"] = "flvmux";
    m_elementNames["wav"] = "wavenc";
    m_elementNames["amr"] = "ffmux_amr";
    m_elementNames["asf"] = "ffmux_asf";
    m_elementNames["dv"] = "ffmux_dv";
    m_elementNames["gif"] = "ffmux_gif";
    m_elementNames["mpeg"] = "ffmux_mpeg";
    m_elementNames["vob"] = "ffmux_vob";
    m_elementNames["mpegts"] = "ffmux_mpegts";
    m_elementNames["3g2"] = "ffmux_3g2";
    m_elementNames["3gp"] = "ffmux_3gp";
    m_elementNames["raw"] = "identity";

    m_containerExtensions["matroska"] = "mkv";
    m_containerExtensions["quicktime"] = "mov";
    m_containerExtensions["mpegts"] = "m2t";
    m_containerExtensions["mpeg"] = "mpg";

    QSet<QString> allTypes;

    for (const QByteArray& formatName : qAsConst(formatCandidates)) {
        QByteArray elementName = m_elementNames[formatName];
        GstElementFactory *factory = gst_element_factory_find(elementName.constData());
        if (factory) {
            m_supportedContainers.append(formatName);
            const gchar *descr = gst_element_factory_get_description(factory);
            m_containerDescriptions.insert(formatName, QString::fromUtf8(descr));


            if (formatName == QByteArray("raw")) {
                m_streamTypes.insert(formatName, allTypes);
            } else {
                QSet<QString> types = supportedStreamTypes(factory, GST_PAD_SINK);
                m_streamTypes.insert(formatName, types);
                allTypes.unite(types);
            }

            gst_object_unref(GST_OBJECT(factory));
        }
    }

    //if (!m_supportedContainers.isEmpty())
    //    setContainerFormat(m_supportedContainers[0]);
}

QSet<QString> QGstreamerMediaContainerControl::supportedStreamTypes(GstElementFactory *factory, GstPadDirection direction)
{
    QSet<QString> types;
    const GList *pads = gst_element_factory_get_static_pad_templates(factory);
    for (const GList *pad = pads; pad; pad = g_list_next(pad)) {
        GstStaticPadTemplate *templ = (GstStaticPadTemplate*)pad->data;
        if (templ->direction == direction) {
            GstCaps *caps = gst_static_caps_get(&templ->static_caps);
            for (uint i=0; i<gst_caps_get_size(caps); i++) {
                GstStructure *structure = gst_caps_get_structure(caps, i);
                types.insert( QString::fromUtf8(gst_structure_get_name(structure)) );
            }
            gst_caps_unref(caps);
        }
    }

    return types;
}


QSet<QString> QGstreamerMediaContainerControl::supportedStreamTypes(const QString &container) const
{
    return m_streamTypes.value(container);
}

QString QGstreamerMediaContainerControl::containerExtension() const
{
    return m_containerExtensions.value(m_format, m_format);
}
