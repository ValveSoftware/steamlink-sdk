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

#include "qaudiosystem.h"
#include "qaudiosystemplugin.h"
#include "qaudiosystempluginext_p.h"

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
    const auto keys = l->keys();
    for (const QString& key : keys) {
        QAudioSystemFactoryInterface* plugin = qobject_cast<QAudioSystemFactoryInterface*>(l->instance(key));
        if (plugin) {
            const auto availableDevices = plugin->availableDevices(mode);
            for (const QByteArray& handle : availableDevices)
                devices << QAudioDeviceInfo(key, handle, mode);
        }
    }
#endif

    return devices;
}

QAudioDeviceInfo QAudioDeviceFactory::defaultDevice(QAudio::Mode mode)
{
#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
    QMediaPluginLoader* l = audioLoader();

    // Check if there is a default plugin.
    QAudioSystemFactoryInterface *plugin = qobject_cast<QAudioSystemFactoryInterface *>(l->instance(defaultKey()));
    if (plugin) {
        // Check if the plugin has the extension interface.
        QAudioSystemPluginExtension *pluginExt = qobject_cast<QAudioSystemPluginExtension *>(l->instance(defaultKey()));
        // Ask for the default device.
        if (pluginExt) {
            const QByteArray &device = pluginExt->defaultDevice(mode);
            if (!device.isEmpty())
                return QAudioDeviceInfo(defaultKey(), device, mode);
        }

        // If there were no default devices, e.g., if the plugin did not implement the extent-ion interface,
        // then just pick the first device that's available.
        const auto &devices = plugin->availableDevices(mode);
        if (!devices.isEmpty())
            return QAudioDeviceInfo(defaultKey(), devices.first(), mode);
    }

    // If no plugin is marked as default, check the other plugins.
    // Note: We're going to prioritize plugins that report a default device.
    const auto &keys = l->keys();
    QAudioDeviceInfo fallbackDevice;
    for (const auto &key : keys) {
        if (key == defaultKey())
            continue;
        QAudioSystemFactoryInterface* plugin = qobject_cast<QAudioSystemFactoryInterface*>(l->instance(key));
        if (plugin) {
            // Check if the plugin has the extent-ion interface.
            QAudioSystemPluginExtension *pluginExt = qobject_cast<QAudioSystemPluginExtension *>(l->instance(key));
            if (pluginExt) {
                const QByteArray &device = pluginExt->defaultDevice(mode);
                if (!device.isEmpty())
                    return QAudioDeviceInfo(key, device, mode);
            } else if (fallbackDevice.isNull()) {
                const auto &devices = plugin->availableDevices(mode);
                if (!devices.isEmpty())
                    fallbackDevice = QAudioDeviceInfo(key, devices.first(), mode);
            }
        }
    }

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
    return createInputDevice(defaultDevice(QAudio::AudioInput), format);
}

QAbstractAudioOutput* QAudioDeviceFactory::createDefaultOutputDevice(QAudioFormat const &format)
{
    return createOutputDevice(defaultDevice(QAudio::AudioOutput), format);
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

