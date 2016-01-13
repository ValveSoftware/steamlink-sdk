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
    pulseEngine->m_defaultSink = info->default_sink_name;
    pulseEngine->m_defaultSource = info->default_source_name;

    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void sinkInfoCallback(pa_context *context, const pa_sink_info *info, int isLast, void *userdata)
{
    QPulseAudioEngine *pulseEngine = static_cast<QPulseAudioEngine*>(userdata);
    QMap<pa_sink_state, QString> stateMap;
    stateMap[PA_SINK_INVALID_STATE] = "n/a";
    stateMap[PA_SINK_RUNNING] = "RUNNING";
    stateMap[PA_SINK_IDLE] = "IDLE";
    stateMap[PA_SINK_SUSPENDED] = "SUSPENDED";

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
    pulseEngine->m_preferredFormats.insert(info->name, format);
    pulseEngine->m_sinks.append(info->name);
}

static void sourceInfoCallback(pa_context *context, const pa_source_info *info, int isLast, void *userdata)
{
    Q_UNUSED(context)
    QPulseAudioEngine *pulseEngine = static_cast<QPulseAudioEngine*>(userdata);

    QMap<pa_source_state, QString> stateMap;
    stateMap[PA_SOURCE_INVALID_STATE] = "n/a";
    stateMap[PA_SOURCE_RUNNING] = "RUNNING";
    stateMap[PA_SOURCE_IDLE] = "IDLE";
    stateMap[PA_SOURCE_SUSPENDED] = "SUSPENDED";

    if (isLast) {
        pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
        return;
    }

    Q_ASSERT(info);

#ifdef DEBUG_PULSE
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
    pulseEngine->m_preferredFormats.insert(info->name, format);
    pulseEngine->m_sources.append(info->name);
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
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);

    // Get output devices
    operation = pa_context_get_sink_info_list(m_context, sinkInfoCallback, this);
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);

    // Get input devices
    operation = pa_context_get_source_info_list(m_context, sourceInfoCallback, this);
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);

    unlock();

    // Swap the default output to index 0
    m_sinks.removeOne(m_defaultSink);
    m_sinks.prepend(m_defaultSink);

    // Swap the default input to index 0
    m_sources.removeOne(m_defaultSource);
    m_sources.prepend(m_defaultSource);
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
    return mode == QAudio::AudioOutput ? m_sinks : m_sources;
}

QT_END_NAMESPACE
