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

#include "qgstreamervideorenderer_p.h"
#include <private/qvideosurfacegstsink_p.h>
#include <private/qgstutils_p.h>
#include <qabstractvideosurface.h>
#include <QtCore/qdebug.h>

#include <gst/gst.h>

QGstreamerVideoRenderer::QGstreamerVideoRenderer(QObject *parent)
    :QVideoRendererControl(parent),m_videoSink(0), m_surface(0)
{
}

QGstreamerVideoRenderer::~QGstreamerVideoRenderer()
{
    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));
}

GstElement *QGstreamerVideoRenderer::videoSink()
{
    if (!m_videoSink && m_surface) {
        m_videoSink = QVideoSurfaceGstSink::createSink(m_surface);
        qt_gst_object_ref_sink(GST_OBJECT(m_videoSink)); //Take ownership
    }

    return reinterpret_cast<GstElement*>(m_videoSink);
}

void QGstreamerVideoRenderer::stopRenderer()
{
    if (m_surface)
        m_surface->stop();
}

QAbstractVideoSurface *QGstreamerVideoRenderer::surface() const
{
    return m_surface;
}

void QGstreamerVideoRenderer::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
        //qDebug() << Q_FUNC_INFO << surface;
        if (m_videoSink)
            gst_object_unref(GST_OBJECT(m_videoSink));

        m_videoSink = 0;

        if (m_surface) {
            disconnect(m_surface.data(), SIGNAL(supportedFormatsChanged()),
                       this, SLOT(handleFormatChange()));
        }

        bool wasReady = isReady();

        m_surface = surface;

        if (m_surface) {
            connect(m_surface.data(), SIGNAL(supportedFormatsChanged()),
                    this, SLOT(handleFormatChange()));
        }

        if (wasReady != isReady())
            emit readyChanged(isReady());

        emit sinkChanged();
    }
}

void QGstreamerVideoRenderer::handleFormatChange()
{
    //qDebug() << "Supported formats list has changed, reload video output";

    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));

    m_videoSink = 0;
    emit sinkChanged();
}
