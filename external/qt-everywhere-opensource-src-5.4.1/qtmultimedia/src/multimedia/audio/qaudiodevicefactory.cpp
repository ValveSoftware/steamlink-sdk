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

#include "qaudiosystem.h"
#include "qaudiosystemplugin.h"

#include "qmediapluginloader_p.h"
#include "qaudiodevicefactory_p.h"

QT_BEGIN_NAMESPACE

static QString defaultKey()
{
    return QStringLiteral("default");
}

#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader, audioLoader,
        (QAudioSystemFactoryInterface_iid, QLatin1String("audio"), Qt::CaseInsensitive))
#endif

class QNullDeviceInfo : public QAbstractAudioDeviceInfo
{
public:
    QAudioFormat preferredFormat() const { qWarning()<<"using null deviceinfo, none available"; return QAudioFormat(); }
    bool isFormatSupported(const QAudioFormat& ) const { return false; }
    QAudioFormat nearestFormat(const QAudioFormat& ) const { return QAudioFormat(); }
    QString deviceName() const { return QString(); }
    QStringList supportedCodecs() { return QStringList(); }
    QList<int> supportedSampleRates()  { return QList<int>(); }
    QList<int> supportedChannelCounts() { return QList<int>(); }
    QList<int> supportedSampleSizes() { return QList<int>(); }
    QList<QAudioFormat::Endian> supportedByteOrders() { return QList<QAudioFormat::Endian>(); }
    QList<QAudioFormat::SampleType> supportedSampleTypes() { return QList<QAudioFormat::SampleType>(); }
};

class QNullInputDevice : public QAbstractAudioInput
{
public:
    void start(QIODevice*) { qWarning()<<"using null input device, none available";}
    QIODevice* start() { qWarning()<<"using null input device, none available"; return 0; }
    void stop() {}
    void reset() {}
    void suspend() {}
    void resume() {}
    int bytesReady() const { return 0; }
    int periodSize() const { return 0; }
    void setBufferSize(int ) {}
    int bufferSize() const  { return 0; }
    void setNotifyInterval(int ) {}
    int notifyInterval() const { return 0; }
    qint64 processedUSecs() const { return 0; }
    qint64 elapsedUSecs() const { return 0; }
    QAudio::Error error() const { return QAudio::OpenError; }
    QAudio::State state() const { return QAudio::StoppedState; }
    void setFormat(const QAudioFormat&) {}
    QAudioFormat format() const { return QAudioFormat(); }
    void setVolume(qreal) {}
    qreal volume() const {return 1.0f;}
};

class QNullOutputDevice : public QAbstractAudioOutput
{
public:
    void start(QIODevice*) {qWarning()<<"using null output device, none available";}
    QIODevice* start() { qWarning()<<"using null output device, none available"; return 0; }
    void stop() {}
    void reset() {}
    void suspend() {}
    void resume() {}
    int bytesFree() const { return 0; }
    int periodSize() const { return 0; }
    void setBufferSize(int ) {}
    int bufferSize() const  { return 0; }
    void setNotifyInterval(int ) {}
    int notifyInterval() const { return 0; }
    qint64 processedUSecs() const { return 0; }
    qint64 elapsedUSecs() const { return 0; }
    QAudio::Error error() const { return QAudio::OpenError; }
    QAudio::State state() const { return QAudio::StoppedState; }
    void setFormat(const QAudioFormat&) {}
    QAudioFormat format() const { return QAudioFormat(); }
};

QList<QAudioDeviceInfo> QAudioDeviceFactory::availableDevices(QAudio::Mode mode)
{
    QList<QAudioDeviceInfo> devices;
#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QMediaPluginLoader* l = audioLoader();
    foreach (const QString& key, l->keys()) {
        QAudioSystemFactoryInterface* plugin = qobject_cast<QAudioSystemFactoryInterface*>(l->instance(key));
        if (plugin) {
            foreach (QByteArray const& handle, plugin->availableDevices(mode))
                devices << QAudioDeviceInfo(key, handle, mode);
        }
    }
#endif

    return devices;
}

QAudioDeviceInfo QAudioDeviceFactory::defaultInputDevice()
{
#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioSystemFactoryInterface* plugin = qobject_cast<QAudioSystemFactoryInterface*>(audioLoader()->instance(defaultKey()));
    if (plugin) {
        QList<QByteArray> list = plugin->availableDevices(QAudio::AudioInput);
        if (list.size() > 0)
            return QAudioDeviceInfo(defaultKey(), list.at(0), QAudio::AudioInput);
    }

    // if no plugin is marked as default or if the default plugin doesn't have any input device,
    // return the first input available from other plugins.
    QList<QAudioDeviceInfo> inputDevices = availableDevices(QAudio::AudioInput);
    if (!inputDevices.isEmpty())
        return inputDevices.first();
#endif

    return QAudioDeviceInfo();
}

QAudioDeviceInfo QAudioDeviceFactory::defaultOutputDevice()
{
#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioSystemFactoryInterface* plugin = qobject_cast<QAudioSystemFactoryInterface*>(audioLoader()->instance(defaultKey()));
    if (plugin) {
        QList<QByteArray> list = plugin->availableDevices(QAudio::AudioOutput);
        if (list.size() > 0)
            return QAudioDeviceInfo(defaultKey(), list.at(0), QAudio::AudioOutput);
    }

    // if no plugin is marked as default or if the default plugin doesn't have any output device,
    // return the first output available from other plugins.
    QList<QAudioDeviceInfo> outputDevices = availableDevices(QAudio::AudioOutput);
    if (!outputDevices.isEmpty())
        return outputDevices.first();
#endif

    return QAudioDeviceInfo();
}

QAbstractAudioDeviceInfo* QAudioDeviceFactory::audioDeviceInfo(const QString &realm, const QByteArray &handle, QAudio::Mode mode)
{
    QAbstractAudioDeviceInfo *rc = 0;

#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioSystemFactoryInterface* plugin =
        qobject_cast<QAudioSystemFactoryInterface*>(audioLoader()->instance(realm));

    if (plugin)
        rc = plugin->createDeviceInfo(handle, mode);
#endif

    return rc == 0 ? new QNullDeviceInfo() : rc;
}

QAbstractAudioInput* QAudioDeviceFactory::createDefaultInputDevice(QAudioFormat const &format)
{
    return createInputDevice(defaultInputDevice(), format);
}

QAbstractAudioOutput* QAudioDeviceFactory::createDefaultOutputDevice(QAudioFormat const &format)
{
    return createOutputDevice(defaultOutputDevice(), format);
}

QAbstractAudioInput* QAudioDeviceFactory::createInputDevice(QAudioDeviceInfo const& deviceInfo, QAudioFormat const &format)
{
    if (deviceInfo.isNull())
        return new QNullInputDevice();

#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioSystemFactoryInterface* plugin =
        qobject_cast<QAudioSystemFactoryInterface*>(audioLoader()->instance(deviceInfo.realm()));

    if (plugin) {
        QAbstractAudioInput* p = plugin->createInput(deviceInfo.handle());
        if (p) p->setFormat(format);
        return p;
    }
#endif

    return new QNullInputDevice();
}

QAbstractAudioOutput* QAudioDeviceFactory::createOutputDevice(QAudioDeviceInfo const& deviceInfo, QAudioFormat const &format)
{
    if (deviceInfo.isNull())
        return new QNullOutputDevice();

#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QAudioSystemFactoryInterface* plugin =
        qobject_cast<QAudioSystemFactoryInterface*>(audioLoader()->instance(deviceInfo.realm()));

    if (plugin) {
        QAbstractAudioOutput* p = plugin->createOutput(deviceInfo.handle());
        if (p) p->setFormat(format);
        return p;
    }
#endif

    return new QNullOutputDevice();
}

QT_END_NAMESPACE

