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

#include <QtCore/qmap.h>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qlist.h>
#include <QtCore/qabstracteventdispatcher.h>
#include <QtCore/qcoreapplication.h>

#include "qgstreamerbushelper_p.h"

QT_BEGIN_NAMESPACE


class QGstreamerBusHelperPrivate : public QObject
{
    Q_OBJECT
public:
    QGstreamerBusHelperPrivate(QGstreamerBusHelper *parent, GstBus* bus) :
        QObject(parent),
        m_tag(0),
        m_bus(bus),
        m_helper(parent),
        m_intervalTimer(Q_NULLPTR)
    {
        // glib event loop can be disabled either by env variable or QT_NO_GLIB define, so check the dispacher
        QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
        const bool hasGlib = dispatcher && dispatcher->inherits("QEventDispatcherGlib");
        if (!hasGlib) {
            m_intervalTimer = new QTimer(this);
            m_intervalTimer->setInterval(250);
            connect(m_intervalTimer, SIGNAL(timeout()), SLOT(interval()));
            m_intervalTimer->start();
        } else {
            m_tag = gst_bus_add_watch_full(bus, G_PRIORITY_DEFAULT, busCallback, this, NULL);
        }
    }

    ~QGstreamerBusHelperPrivate()
    {
        m_helper = 0;
        delete m_intervalTimer;

        if (m_tag)
            g_source_remove(m_tag);
    }

    GstBus* bus() const { return m_bus; }

private slots:
    void interval()
    {
        GstMessage* message;
        while ((message = gst_bus_poll(m_bus, GST_MESSAGE_ANY, 0)) != 0) {
            processMessage(message);
            gst_message_unref(message);
        }
    }

private:
    void processMessage(GstMessage* message)
    {
        QGstreamerMessage msg(message);
        doProcessMessage(msg);
    }

    void queueMessage(GstMessage* message)
    {
        QGstreamerMessage msg(message);
        QMetaObject::invokeMethod(this, "doProcessMessage", Qt::QueuedConnection,
                                  Q_ARG(QGstreamerMessage, msg));
    }

    static gboolean busCallback(GstBus *bus, GstMessage *message, gpointer data)
    {
        Q_UNUSED(bus);
        reinterpret_cast<QGstreamerBusHelperPrivate*>(data)->queueMessage(message);
        return TRUE;
    }

    guint m_tag;
    GstBus* m_bus;
    QGstreamerBusHelper*  m_helper;
    QTimer*     m_intervalTimer;

private slots:
    void doProcessMessage(const QGstreamerMessage& msg)
    {
        for (QGstreamerBusMessageFilter *filter : qAsConst(busFilters)) {
            if (filter->processBusMessage(msg))
                break;
        }
        emit m_helper->message(msg);
    }

public:
    QMutex filterMutex;
    QList<QGstreamerSyncMessageFilter*> syncFilters;
    QList<QGstreamerBusMessageFilter*> busFilters;
};


static GstBusSyncReply syncGstBusFilter(GstBus* bus, GstMessage* message, QGstreamerBusHelperPrivate *d)
{
    Q_UNUSED(bus);
    QMutexLocker lock(&d->filterMutex);

    for (QGstreamerSyncMessageFilter *filter : qAsConst(d->syncFilters)) {
        if (filter->processSyncMessage(QGstreamerMessage(message)))
            return GST_BUS_DROP;
    }

    return GST_BUS_PASS;
}


/*!
    \class QGstreamerBusHelper
    \internal
*/

QGstreamerBusHelper::QGstreamerBusHelper(GstBus* bus, QObject* parent):
    QObject(parent)
{
    d = new QGstreamerBusHelperPrivate(this, bus);
#if GST_CHECK_VERSION(1,0,0)
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, d, 0);
#else
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, d);
#endif
    gst_object_ref(GST_OBJECT(bus));
}

QGstreamerBusHelper::~QGstreamerBusHelper()
{
#if GST_CHECK_VERSION(1,0,0)
    gst_bus_set_sync_handler(d->bus(), 0, 0, 0);
#else
    gst_bus_set_sync_handler(d->bus(),0,0);
#endif
    gst_object_unref(GST_OBJECT(d->bus()));
}

void QGstreamerBusHelper::installMessageFilter(QObject *filter)
{
    QGstreamerSyncMessageFilter *syncFilter = qobject_cast<QGstreamerSyncMessageFilter*>(filter);
    if (syncFilter) {
        QMutexLocker lock(&d->filterMutex);
        if (!d->syncFilters.contains(syncFilter))
            d->syncFilters.append(syncFilter);
    }

    QGstreamerBusMessageFilter *busFilter = qobject_cast<QGstreamerBusMessageFilter*>(filter);
    if (busFilter && !d->busFilters.contains(busFilter))
        d->busFilters.append(busFilter);
}

void QGstreamerBusHelper::removeMessageFilter(QObject *filter)
{
    QGstreamerSyncMessageFilter *syncFilter = qobject_cast<QGstreamerSyncMessageFilter*>(filter);
    if (syncFilter) {
        QMutexLocker lock(&d->filterMutex);
        d->syncFilters.removeAll(syncFilter);
    }

    QGstreamerBusMessageFilter *busFilter = qobject_cast<QGstreamerBusMessageFilter*>(filter);
    if (busFilter)
        d->busFilters.removeAll(busFilter);
}

QT_END_NAMESPACE

#include "qgstreamerbushelper.moc"
