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

#include <QtCore/qmap.h>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qlist.h>

#include "qgstreamerbushelper_p.h"

QT_BEGIN_NAMESPACE


class QGstreamerBusHelperPrivate : public QObject
{
    Q_OBJECT
public:
    QGstreamerBusHelperPrivate(QGstreamerBusHelper *parent, GstBus* bus) :
        QObject(parent),
        m_bus(bus),
        m_helper(parent)
    {
#ifdef QT_NO_GLIB
        Q_UNUSED(bus);

        m_intervalTimer = new QTimer(this);
        m_intervalTimer->setInterval(250);

        connect(m_intervalTimer, SIGNAL(timeout()), SLOT(interval()));
        m_intervalTimer->start();
#else
        m_tag = gst_bus_add_watch_full(bus, 0, busCallback, this, NULL);
#endif

    }

    ~QGstreamerBusHelperPrivate()
    {
        m_helper = 0;
#ifdef QT_NO_GLIB
        m_intervalTimer->stop();
#else
        g_source_remove(m_tag);
#endif
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
#ifdef QT_NO_GLIB
    QTimer*     m_intervalTimer;
#endif

private slots:
    void doProcessMessage(const QGstreamerMessage& msg)
    {
        foreach (QGstreamerBusMessageFilter *filter, busFilters) {
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

    foreach (QGstreamerSyncMessageFilter *filter, d->syncFilters) {
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
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)syncGstBusFilter, d);
    gst_object_ref(GST_OBJECT(bus));
}

QGstreamerBusHelper::~QGstreamerBusHelper()
{
    gst_bus_set_sync_handler(d->bus(),0,0);
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
