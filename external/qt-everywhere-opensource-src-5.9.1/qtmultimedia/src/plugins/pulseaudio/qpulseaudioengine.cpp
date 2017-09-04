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

#include <QtCore/qdebug.h>

#include <qaudiodeviceinfo.h>
#include "qpulseaudioengine.h"
#include "qaudiodeviceinfo_pulse.h"
#include "qaudiooutput_pulse.h"
#include "qpulsehelpers.h"
#include <sys/types.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

static void serverInfoCallback(pa_context *context, const pa_server_info *info, void *userdata)
{
    if (!info) {
        qWarning() << QString("Failed to get server information: %s").arg(pa_strerror(pa_context_errno(context)));
        return;
    }

#ifdef DEBUG_PULSE
    char ss[PA_SAMPLE_SPEC_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];

    pa_sample_spec_snprint(ss, sizeof(ss), &info->sample_spec);
    pa_channel_map_snprint(cm, sizeof(cm), &info->channel_map);

    qDebug() << QString("User name: %1\n"
             "Host Name: %2\n"
             "Server Name: %3\n"
             "Server Version: %4\n"
             "Default Sample Specification: %5\n"
             "Default Channel Map: %6\n"
             "Default Sink: %7\n"
             "Default Source: %8\n").arg(
           info->user_name,
           info->host_name,
           info->server_name,
           info->server_version,
           ss,
           cm,
           info->default_sink_name,
           info->default_source_name);
#endif

    QPulseAudioEngine *pulseEngine = static_cast<QPulseAudioEngine*>(userdata);
    pulseEngine->m_serverLock.lockForWrite();
    pulseEngine->m_defaultSink = info->default_sink_name;
    pulseEngine->m_defaultSource = info->default_source_name;
    pulseEngine->m_serverLock.unlock();

    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void sinkInfoCallback(pa_context *context, const pa_sink_info *info, int isLast, void *userdata)
{
    QPulseAudioEngine *pulseEngine = static_cast<QPulseAudioEngine*>(userdata);

    if (isLast < 0) {
        qWarning() << QString("Failed to get sink information: %s").arg(pa_strerror(pa_context_errno(context)));
        return;
    }

    if (isLast) {
        pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
        return;
    }

    Q_ASSERT(info);

#ifdef DEBUG_PULSE
    QMap<pa_sink_state, QString> stateMap;
    stateMap[PA_SINK_INVALID_STATE] = "n/a";
    stateMap[PA_SINK_RUNNING] = "RUNNING";
    stateMap[PA_SINK_IDLE] = "IDLE";
    stateMap[PA_SINK_SUSPENDED] = "SUSPENDED";

    qDebug() << QString("Sink #%1\n"
             "\tState: %2\n"
             "\tName: %3\n"
             "\tDescription: %4\n"
            ).arg(QString::number(info->index),
                  stateMap.value(info->state),
                  info->name,
                  info->description);
#endif

    QAudioFormat format = QPulseAudioInternal::sampleSpecToAudioFormat(info->sample_spec);

    QWriteLocker locker(&pulseEngine->m_sinkLock);
    pulseEngine->m_preferredFormats.insert(info->name, format);
    pulseEngine->m_sinks.insert(info->index, info->name);
}

static void sourceInfoCallback(pa_context *context, const pa_source_info *info, int isLast, void *userdata)
{
    Q_UNUSED(context)
    QPulseAudioEngine *pulseEngine = static_cast<QPulseAudioEngine*>(userdata);

    if (isLast) {
        pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
        return;
    }

    Q_ASSERT(info);

#ifdef DEBUG_PULSE
    QMap<pa_source_state, QString> stateMap;
    stateMap[PA_SOURCE_INVALID_STATE] = "n/a";
    stateMap[PA_SOURCE_RUNNING] = "RUNNING";
    stateMap[PA_SOURCE_IDLE] = "IDLE";
    stateMap[PA_SOURCE_SUSPENDED] = "SUSPENDED";

    qDebug() << QString("Source #%1\n"
         "\tState: %2\n"
         "\tName: %3\n"
         "\tDescription: %4\n"
        ).arg(QString::number(info->index),
              stateMap.value(info->state),
              info->name,
              info->description);
#endif

    QAudioFormat format = QPulseAudioInternal::sampleSpecToAudioFormat(info->sample_spec);

    QWriteLocker locker(&pulseEngine->m_sourceLock);
    pulseEngine->m_preferredFormats.insert(info->name, format);
    pulseEngine->m_sources.insert(info->index, info->name);
}

static void event_cb(pa_context* context, pa_subscription_event_type_t t, uint32_t index, void* userdata)
{
    QPulseAudioEngine *pulseEngine = static_cast<QPulseAudioEngine*>(userdata);

    int type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    int facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

    switch (type) {
    case PA_SUBSCRIPTION_EVENT_NEW:
    case PA_SUBSCRIPTION_EVENT_CHANGE:
        switch (facility) {
        case PA_SUBSCRIPTION_EVENT_SERVER: {
            pa_operation *op = pa_context_get_server_info(context, serverInfoCallback, userdata);
            if (op)
                pa_operation_unref(op);
            else
                qWarning("PulseAudioService: failed to get server info");
            break;
        }
        case PA_SUBSCRIPTION_EVENT_SINK: {
            pa_operation *op = pa_context_get_sink_info_by_index(context, index, sinkInfoCallback, userdata);
            if (op)
                pa_operation_unref(op);
            else
                qWarning("PulseAudioService: failed to get sink info");
            break;
        }
        case PA_SUBSCRIPTION_EVENT_SOURCE: {
            pa_operation *op = pa_context_get_source_info_by_index(context, index, sourceInfoCallback, userdata);
            if (op)
                pa_operation_unref(op);
            else
                qWarning("PulseAudioService: failed to get source info");
            break;
        }
        default:
            break;
        }
        break;
    case PA_SUBSCRIPTION_EVENT_REMOVE:
        switch (facility) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            pulseEngine->m_sinkLock.lockForWrite();
            pulseEngine->m_preferredFormats.remove(pulseEngine->m_sinks.value(index));
            pulseEngine->m_sinks.remove(index);
            pulseEngine->m_sinkLock.unlock();
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            pulseEngine->m_sourceLock.lockForWrite();
            pulseEngine->m_preferredFormats.remove(pulseEngine->m_sources.value(index));
            pulseEngine->m_sources.remove(index);
            pulseEngine->m_sourceLock.unlock();
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

static void contextStateCallbackInit(pa_context *context, void *userdata)
{
    Q_UNUSED(context);
#ifdef DEBUG_PULSE
    qDebug() << QPulseAudioInternal::stateToQString(pa_context_get_state(context));
#endif
    QPulseAudioEngine *pulseEngine = reinterpret_cast<QPulseAudioEngine*>(userdata);
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void contextStateCallback(pa_context *c, void *userdata)
{
    QPulseAudioEngine *self = reinterpret_cast<QPulseAudioEngine*>(userdata);
    pa_context_state_t state = pa_context_get_state(c);

#ifdef DEBUG_PULSE
    qDebug() << QPulseAudioInternal::stateToQString(state);
#endif

    if (state == PA_CONTEXT_FAILED)
        QMetaObject::invokeMethod(self, "onContextFailed", Qt::QueuedConnection);
}

Q_GLOBAL_STATIC(QPulseAudioEngine, pulseEngine);

QPulseAudioEngine::QPulseAudioEngine(QObject *parent)
    : QObject(parent)
    , m_mainLoopApi(0)
    , m_context(0)
    , m_prepared(false)
{
    prepare();
}

QPulseAudioEngine::~QPulseAudioEngine()
{
    if (m_prepared)
        release();
}

void QPulseAudioEngine::prepare()
{
    bool keepGoing = true;
    bool ok = true;

    m_mainLoop = pa_threaded_mainloop_new();
    if (m_mainLoop == 0) {
        qWarning("PulseAudioService: unable to create pulseaudio mainloop");
        return;
    }

    if (pa_threaded_mainloop_start(m_mainLoop) != 0) {
        qWarning("PulseAudioService: unable to start pulseaudio mainloop");
        pa_threaded_mainloop_free(m_mainLoop);
        m_mainLoop = 0;
        return;
    }

    m_mainLoopApi = pa_threaded_mainloop_get_api(m_mainLoop);

    lock();

    m_context = pa_context_new(m_mainLoopApi, QString(QLatin1String("QtPulseAudio:%1")).arg(::getpid()).toLatin1().constData());

    if (m_context == 0) {
        qWarning("PulseAudioService: Unable to create new pulseaudio context");
        pa_threaded_mainloop_unlock(m_mainLoop);
        pa_threaded_mainloop_free(m_mainLoop);
        m_mainLoop = 0;
        onContextFailed();
        return;
    }

    pa_context_set_state_callback(m_context, contextStateCallbackInit, this);

    if (pa_context_connect(m_context, 0, (pa_context_flags_t)0, 0) < 0) {
        qWarning("PulseAudioService: pa_context_connect() failed");
        pa_context_unref(m_context);
        pa_threaded_mainloop_unlock(m_mainLoop);
        pa_threaded_mainloop_free(m_mainLoop);
        m_mainLoop = 0;
        m_context = 0;
        return;
    }

    pa_threaded_mainloop_wait(m_mainLoop);

    while (keepGoing) {
        switch (pa_context_get_state(m_context)) {
            case PA_CONTEXT_CONNECTING:
            case PA_CONTEXT_AUTHORIZING:
            case PA_CONTEXT_SETTING_NAME:
                break;

            case PA_CONTEXT_READY:
#ifdef DEBUG_PULSE
                qDebug("Connection established.");
#endif
                keepGoing = false;
                break;

            case PA_CONTEXT_TERMINATED:
                qCritical("PulseAudioService: Context terminated.");
                keepGoing = false;
                ok = false;
                break;

            case PA_CONTEXT_FAILED:
            default:
                qCritical() << QString("PulseAudioService: Connection failure: %1").arg(pa_strerror(pa_context_errno(m_context)));
                keepGoing = false;
                ok = false;
        }

        if (keepGoing)
            pa_threaded_mainloop_wait(m_mainLoop);
    }

    if (ok) {
        pa_context_set_state_callback(m_context, contextStateCallback, this);

        pa_context_set_subscribe_callback(m_context, event_cb, this);
        pa_operation *op = pa_context_subscribe(m_context,
                                                pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SINK |
                                                                       PA_SUBSCRIPTION_MASK_SOURCE |
                                                                       PA_SUBSCRIPTION_MASK_SERVER),
                                                NULL, NULL);
        if (op)
            pa_operation_unref(op);
        else
            qWarning("PulseAudioService: failed to subscribe to context notifications");
    } else {
        pa_context_unref(m_context);
        m_context = 0;
    }

    unlock();

    if (ok) {
        updateDevices();
        m_prepared = true;
    } else {
        pa_threaded_mainloop_free(m_mainLoop);
        m_mainLoop = 0;
        onContextFailed();
    }
}

void QPulseAudioEngine::release()
{
    if (!m_prepared)
        return;

    if (m_context) {
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = 0;
    }

    if (m_mainLoop) {
        pa_threaded_mainloop_stop(m_mainLoop);
        pa_threaded_mainloop_free(m_mainLoop);
        m_mainLoop = 0;
    }

    m_prepared = false;
}

void QPulseAudioEngine::updateDevices()
{
    lock();

    // Get default input and output devices
    pa_operation *operation = pa_context_get_server_info(m_context, serverInfoCallback, this);
    if (operation) {
        while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(m_mainLoop);
        pa_operation_unref(operation);
    } else {
        qWarning("PulseAudioService: failed to get server info");
    }

    // Get output devices
    operation = pa_context_get_sink_info_list(m_context, sinkInfoCallback, this);
    if (operation) {
        while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(m_mainLoop);
        pa_operation_unref(operation);
    } else {
        qWarning("PulseAudioService: failed to get sink info");
    }

    // Get input devices
    operation = pa_context_get_source_info_list(m_context, sourceInfoCallback, this);
    if (operation) {
        while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(m_mainLoop);
        pa_operation_unref(operation);
    } else {
        qWarning("PulseAudioService: failed to get source info");
    }

    unlock();
}

void QPulseAudioEngine::onContextFailed()
{
    // Give a chance to the connected slots to still use the Pulse main loop before releasing it.
    emit contextFailed();

    release();

    // Try to reconnect later
    QTimer::singleShot(3000, this, SLOT(prepare()));
}

QPulseAudioEngine *QPulseAudioEngine::instance()
{
    return pulseEngine();
}

QList<QByteArray> QPulseAudioEngine::availableDevices(QAudio::Mode mode) const
{
    QList<QByteArray> devices;
    QByteArray defaultDevice;

    m_serverLock.lockForRead();

    if (mode == QAudio::AudioOutput) {
        QReadLocker locker(&m_sinkLock);
        devices = m_sinks.values();
        defaultDevice = m_defaultSink;
    } else {
        QReadLocker locker(&m_sourceLock);
        devices = m_sources.values();
        defaultDevice = m_defaultSource;
    }

    m_serverLock.unlock();

    // Swap the default device to index 0
    devices.removeOne(defaultDevice);
    devices.prepend(defaultDevice);

    return devices;
}

QByteArray QPulseAudioEngine::defaultDevice(QAudio::Mode mode) const
{
    return (mode == QAudio::AudioOutput) ? m_defaultSink : m_defaultSource;
}

QT_END_NAMESPACE
