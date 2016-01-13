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

#include <QDebug>

#include "qgstappsrc_p.h"
#include <QtNetwork>

QGstAppSrc::QGstAppSrc(QObject *parent)
    :QObject(parent)
    ,m_stream(0)
    ,m_appSrc(0)
    ,m_sequential(false)
    ,m_maxBytes(0)
    ,m_setup(false)
    ,m_dataRequestSize(~0)
    ,m_dataRequested(false)
    ,m_enoughData(false)
    ,m_forceData(false)
{
    m_callbacks.need_data   = &QGstAppSrc::on_need_data;
    m_callbacks.enough_data = &QGstAppSrc::on_enough_data;
    m_callbacks.seek_data   = &QGstAppSrc::on_seek_data;
}

QGstAppSrc::~QGstAppSrc()
{
    if (m_appSrc)
        gst_object_unref(G_OBJECT(m_appSrc));
}

bool QGstAppSrc::setup(GstElement* appsrc)
{
    if (m_setup || m_stream == 0 || appsrc == 0)
        return false;

    if (m_appSrc)
        gst_object_unref(G_OBJECT(m_appSrc));

    m_appSrc = GST_APP_SRC(appsrc);
    gst_object_ref(G_OBJECT(m_appSrc));
    gst_app_src_set_callbacks(m_appSrc, (GstAppSrcCallbacks*)&m_callbacks, this, (GDestroyNotify)&QGstAppSrc::destroy_notify);

    g_object_get(G_OBJECT(m_appSrc), "max-bytes", &m_maxBytes, NULL);

    if (m_sequential)
        m_streamType = GST_APP_STREAM_TYPE_STREAM;
    else
        m_streamType = GST_APP_STREAM_TYPE_RANDOM_ACCESS;
    gst_app_src_set_stream_type(m_appSrc, m_streamType);
    gst_app_src_set_size(m_appSrc, (m_sequential) ? -1 : m_stream->size());

    return  m_setup = true;
}

void QGstAppSrc::setStream(QIODevice *stream)
{
    if (stream == 0)
        return;
    if (m_stream) {
        disconnect(m_stream, SIGNAL(readyRead()), this, SLOT(onDataReady()));
        disconnect(m_stream, SIGNAL(destroyed()), this, SLOT(streamDestroyed()));
    }
    if (m_appSrc)
        gst_object_unref(G_OBJECT(m_appSrc));

    m_dataRequestSize = ~0;
    m_dataRequested = false;
    m_enoughData = false;
    m_forceData = false;
    m_maxBytes = 0;

    m_appSrc = 0;
    m_stream = stream;
    connect(m_stream, SIGNAL(destroyed()), SLOT(streamDestroyed()));
    connect(m_stream, SIGNAL(readyRead()), this, SLOT(onDataReady()));
    m_sequential = m_stream->isSequential();
    m_setup = false;
}

QIODevice *QGstAppSrc::stream() const
{
    return m_stream;
}

GstAppSrc *QGstAppSrc::element()
{
    return m_appSrc;
}

void QGstAppSrc::onDataReady()
{
    if (!m_enoughData) {
        m_dataRequested = true;
        pushDataToAppSrc();
    }
}

void QGstAppSrc::streamDestroyed()
{
    if (sender() == m_stream) {
        m_stream = 0;
        sendEOS();
    }
}

void QGstAppSrc::pushDataToAppSrc()
{
    if (!isStreamValid() || !m_setup)
        return;

    if (m_dataRequested && !m_enoughData) {
        qint64 size;
        if (m_dataRequestSize == ~0u)
            size = qMin(m_stream->bytesAvailable(), queueSize());
        else
            size = qMin(m_stream->bytesAvailable(), (qint64)m_dataRequestSize);

        if (size) {
            void *data = g_malloc(size);
            GstBuffer* buffer = gst_app_buffer_new(data, size, g_free, data);
            buffer->offset = m_stream->pos();
            qint64 bytesRead = m_stream->read((char*)GST_BUFFER_DATA(buffer), size);
            buffer->offset_end =  buffer->offset + bytesRead - 1;

            if (bytesRead > 0) {
                m_dataRequested = false;
                m_enoughData = false;
                GstFlowReturn ret = gst_app_src_push_buffer (GST_APP_SRC (element()), buffer);
                if (ret == GST_FLOW_ERROR) {
                    qWarning()<<"appsrc: push buffer error";
                } else if (ret == GST_FLOW_WRONG_STATE) {
                    qWarning()<<"appsrc: push buffer wrong state";
                } else if (ret == GST_FLOW_RESEND) {
                    qWarning()<<"appsrc: push buffer resend";
                }
            }
        } else {
            sendEOS();
        }
    } else if (m_stream->atEnd()) {
        sendEOS();
    }
}

bool QGstAppSrc::doSeek(qint64 value)
{
    if (isStreamValid())
        return stream()->seek(value);
    return false;
}


gboolean QGstAppSrc::on_seek_data(GstAppSrc *element, guint64 arg0, gpointer userdata)
{
    Q_UNUSED(element);
    QGstAppSrc *self = reinterpret_cast<QGstAppSrc*>(userdata);
    if (self && self->isStreamValid()) {
        if (!self->stream()->isSequential())
            QMetaObject::invokeMethod(self, "doSeek", Qt::AutoConnection, Q_ARG(qint64, arg0));
    }
    else
        return false;

    return true;
}

void QGstAppSrc::on_enough_data(GstAppSrc *element, gpointer userdata)
{
    Q_UNUSED(element);
    QGstAppSrc *self = reinterpret_cast<QGstAppSrc*>(userdata);
    if (self)
        self->enoughData() = true;
}

void QGstAppSrc::on_need_data(GstAppSrc *element, guint arg0, gpointer userdata)
{
    Q_UNUSED(element);
    QGstAppSrc *self = reinterpret_cast<QGstAppSrc*>(userdata);
    if (self) {
        self->dataRequested() = true;
        self->enoughData() = false;
        self->dataRequestSize()= arg0;
        QMetaObject::invokeMethod(self, "pushDataToAppSrc", Qt::AutoConnection);
    }
}

void QGstAppSrc::destroy_notify(gpointer data)
{
    Q_UNUSED(data);
}

void QGstAppSrc::sendEOS()
{
    gst_app_src_end_of_stream(GST_APP_SRC(m_appSrc));
    if (isStreamValid() && !stream()->isSequential())
        stream()->reset();
}
