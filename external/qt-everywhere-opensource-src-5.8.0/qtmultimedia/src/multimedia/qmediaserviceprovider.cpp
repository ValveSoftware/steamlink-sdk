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
#include <QtCore/qmap.h>

#include "qmediaservice.h"
#include "qmediaserviceprovider_p.h"
#include "qmediaserviceproviderplugin.h"
#include "qmediapluginloader_p.h"
#include "qmediaplayer.h"

QT_BEGIN_NAMESPACE

QMediaServiceProviderFactoryInterface::~QMediaServiceProviderFactoryInterface()
{
}

class QMediaServiceProviderHintPrivate : public QSharedData
{
public:
    QMediaServiceProviderHintPrivate(QMediaServiceProviderHint::Type type)
        :type(type), cameraPosition(QCamera::UnspecifiedPosition), features(0)
    {
    }

    QMediaServiceProviderHintPrivate(const QMediaServiceProviderHintPrivate &other)
        :QSharedData(other),
        type(other.type),
        device(other.device),
        cameraPosition(other.cameraPosition),
        mimeType(other.mimeType),
        codecs(other.codecs),
        features(other.features)
    {
    }

    ~QMediaServiceProviderHintPrivate()
    {
    }

    QMediaServiceProviderHint::Type type;
    QByteArray device;
    QCamera::Position cameraPosition;
    QString mimeType;
    QStringList codecs;
    QMediaServiceProviderHint::Features features;
};

/*!
    \class QMediaServiceProviderHint

    \brief The QMediaServiceProviderHint class describes what is required of a QMediaService.

    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_control
    \ingroup multimedia_core

    \internal

    The QMediaServiceProvider class uses hints to select an appropriate media service.
*/

/*!
    \enum QMediaServiceProviderHint::Feature

    Enumerates features a media service may provide.

    \value LowLatencyPlayback
            The service is expected to play simple audio formats,
            but playback should start without significant delay.
            Such playback service can be used for beeps, ringtones, etc.

    \value RecordingSupport
            The service provides audio or video recording functions.

    \value StreamPlayback
            The service is capable of playing QIODevice based streams.

    \value VideoSurface
            The service is capable of renderering to a QAbstractVideoSurface
            output.
*/

/*!
    \enum QMediaServiceProviderHint::Type

    Enumerates the possible types of media service provider hint.

    \value Null               En empty hint, use the default service.
    \value ContentType        Select media service most suitable for certain content type.
    \value Device             Select media service which supports certain device.
    \value SupportedFeatures  Select media service supporting the set of optional features.
    \value CameraPosition     Select media service having a camera at a specified position.
*/


/*!
    Constructs an empty media service provider hint.
*/
QMediaServiceProviderHint::QMediaServiceProviderHint()
    :d(new QMediaServiceProviderHintPrivate(Null))
{
}

/*!
    Constructs a ContentType media service provider hint.

    This type of hint describes a service that is able to play content of a specific MIME \a type
    encoded with one or more of the listed \a codecs.
*/
QMediaServiceProviderHint::QMediaServiceProviderHint(const QString &type, const QStringList& codecs)
    :d(new QMediaServiceProviderHintPrivate(ContentType))
{
    d->mimeType = type;
    d->codecs = codecs;
}

/*!
  Constructs a Device media service provider hint.

  This type of hint describes a media service that utilizes a specific \a device.
*/
QMediaServiceProviderHint::QMediaServiceProviderHint(const QByteArray &device)
    :d(new QMediaServiceProviderHintPrivate(Device))
{
    d->device = device;
}

/*!
  \since 5.3

  Constructs a CameraPosition media service provider hint.

  This type of hint describes a media service that has a camera in the specific \a position.
*/
QMediaServiceProviderHint::QMediaServiceProviderHint(QCamera::Position position)
    :d(new QMediaServiceProviderHintPrivate(CameraPosition))
{
    d->cameraPosition = position;
}

/*!
    Constructs a SupportedFeatures media service provider hint.

    This type of hint describes a service which supports a specific set of \a features.
*/
QMediaServiceProviderHint::QMediaServiceProviderHint(QMediaServiceProviderHint::Features features)
    :d(new QMediaServiceProviderHintPrivate(SupportedFeatures))
{
    d->features = features;
}

/*!
    Constructs a copy of the media service provider hint \a other.
*/
QMediaServiceProviderHint::QMediaServiceProviderHint(const QMediaServiceProviderHint &other)
    :d(other.d)
{
}

/*!
    Destroys a media service provider hint.
*/
QMediaServiceProviderHint::~QMediaServiceProviderHint()
{
}

/*!
    Assigns the value \a other to a media service provider hint.
*/
QMediaServiceProviderHint& QMediaServiceProviderHint::operator=(const QMediaServiceProviderHint &other)
{
    d = other.d;
    return *this;
}

/*!
    Identifies if \a other is of equal value to a media service provider hint.

    Returns true if the hints are equal, and false if they are not.
*/
bool QMediaServiceProviderHint::operator == (const QMediaServiceProviderHint &other) const
{
    return (d == other.d) ||
           (d->type == other.d->type &&
            d->device == other.d->device &&
            d->cameraPosition == other.d->cameraPosition &&
            d->mimeType == other.d->mimeType &&
            d->codecs == other.d->codecs &&
            d->features == other.d->features);
}

/*!
    Identifies if \a other is not of equal value to a media service provider hint.

    Returns true if the hints are not equal, and false if they are.
*/
bool QMediaServiceProviderHint::operator != (const QMediaServiceProviderHint &other) const
{
    return !(*this == other);
}

/*!
    Returns true if a media service provider is null.
*/
bool QMediaServiceProviderHint::isNull() const
{
    return d->type == Null;
}

/*!
    Returns the type of a media service provider hint.
*/
QMediaServiceProviderHint::Type QMediaServiceProviderHint::type() const
{
    return d->type;
}

/*!
    Returns the mime type of the media a service is expected to be able play.
*/
QString QMediaServiceProviderHint::mimeType() const
{
    return d->mimeType;
}

/*!
    Returns a list of codes a media service is expected to be able to decode.
*/
QStringList QMediaServiceProviderHint::codecs() const
{
    return d->codecs;
}

/*!
    Returns the name of a device a media service is expected to utilize.
*/
QByteArray QMediaServiceProviderHint::device() const
{
    return d->device;
}

/*!
    \since 5.3

    Returns the camera's position a media service is expected to utilize.
*/
QCamera::Position QMediaServiceProviderHint::cameraPosition() const
{
    return d->cameraPosition;
}


/*!
    Returns a set of features a media service is expected to provide.
*/
QMediaServiceProviderHint::Features QMediaServiceProviderHint::features() const
{
    return d->features;
}


Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader, loader,
        (QMediaServiceProviderFactoryInterface_iid, QLatin1String("mediaservice"), Qt::CaseInsensitive))


class QPluginServiceProvider : public QMediaServiceProvider
{
    struct MediaServiceData {
        QByteArray type;
        QMediaServiceProviderPlugin *plugin;

        MediaServiceData() : plugin(0) { }
    };

    QMap<const QMediaService*, MediaServiceData> mediaServiceData;

public:
    QMediaService* requestService(const QByteArray &type, const QMediaServiceProviderHint &hint)
    {
        QString key(QLatin1String(type.constData()));

        QList<QMediaServiceProviderPlugin *>plugins;
        const auto instances = loader()->instances(key);
        for (QObject *obj : instances) {
            QMediaServiceProviderPlugin *plugin =
                qobject_cast<QMediaServiceProviderPlugin*>(obj);
            if (plugin)
                plugins << plugin;
        }

        if (!plugins.isEmpty()) {
            QMediaServiceProviderPlugin *plugin = 0;

            switch (hint.type()) {
            case QMediaServiceProviderHint::Null:
                plugin = plugins[0];
                //special case for media player, if low latency was not asked,
                //prefer services not offering it, since they are likely to support
                //more formats
                if (type == QByteArray(Q_MEDIASERVICE_MEDIAPLAYER)) {
                    for (QMediaServiceProviderPlugin *currentPlugin : qAsConst(plugins)) {
                        QMediaServiceFeaturesInterface *iface =
                                qobject_cast<QMediaServiceFeaturesInterface*>(currentPlugin);

                        if (!iface || !(iface->supportedFeatures(type) &
                                        QMediaServiceProviderHint::LowLatencyPlayback)) {
                            plugin = currentPlugin;
                            break;
                        }

                    }
                }
                break;
            case QMediaServiceProviderHint::SupportedFeatures:
                plugin = plugins[0];
                for (QMediaServiceProviderPlugin *currentPlugin : qAsConst(plugins)) {
                    QMediaServiceFeaturesInterface *iface =
                            qobject_cast<QMediaServiceFeaturesInterface*>(currentPlugin);

                    if (iface) {
                        if ((iface->supportedFeatures(type) & hint.features()) == hint.features()) {
                            plugin = currentPlugin;
                            break;
                        }
                    }
                }
                break;
            case QMediaServiceProviderHint::Device: {
                    plugin = plugins[0];
                    for (QMediaServiceProviderPlugin *currentPlugin : qAsConst(plugins)) {
                        QMediaServiceSupportedDevicesInterface *iface =
                                qobject_cast<QMediaServiceSupportedDevicesInterface*>(currentPlugin);

                        if (iface && iface->devices(type).contains(hint.device())) {
                            plugin = currentPlugin;
                            break;
                        }
                    }
                }
                break;
            case QMediaServiceProviderHint::CameraPosition: {
                    plugin = plugins[0];
                    if (type == QByteArray(Q_MEDIASERVICE_CAMERA)
                            && hint.cameraPosition() != QCamera::UnspecifiedPosition) {
                        for (QMediaServiceProviderPlugin *currentPlugin : qAsConst(plugins)) {
                            const QMediaServiceSupportedDevicesInterface *deviceIface =
                                    qobject_cast<QMediaServiceSupportedDevicesInterface*>(currentPlugin);
                            const QMediaServiceCameraInfoInterface *cameraIface =
                                    qobject_cast<QMediaServiceCameraInfoInterface*>(currentPlugin);

                            if (deviceIface && cameraIface) {
                                const QList<QByteArray> cameras = deviceIface->devices(type);
                                for (const QByteArray &camera : cameras) {
                                    if (cameraIface->cameraPosition(camera) == hint.cameraPosition()) {
                                        plugin = currentPlugin;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            case QMediaServiceProviderHint::ContentType: {
                    QMultimedia::SupportEstimate estimate = QMultimedia::NotSupported;
                    for (QMediaServiceProviderPlugin *currentPlugin : qAsConst(plugins)) {
                        QMultimedia::SupportEstimate currentEstimate = QMultimedia::MaybeSupported;
                        QMediaServiceSupportedFormatsInterface *iface =
                                qobject_cast<QMediaServiceSupportedFormatsInterface*>(currentPlugin);

                        if (iface)
                            currentEstimate = iface->hasSupport(hint.mimeType(), hint.codecs());

                        if (currentEstimate > estimate) {
                            estimate = currentEstimate;
                            plugin = currentPlugin;

                            if (currentEstimate == QMultimedia::PreferredService)
                                break;
                        }
                    }
                }
                break;
            }

            if (plugin != 0) {
                QMediaService *service = plugin->create(key);
                if (service != 0) {
                    MediaServiceData d;
                    d.type = type;
                    d.plugin = plugin;
                    mediaServiceData.insert(service, d);
                }

                return service;
            }
        }

        qWarning() << "defaultServiceProvider::requestService(): no service found for -" << key;
        return 0;
    }

    void releaseService(QMediaService *service)
    {
        if (service != 0) {
            MediaServiceData d = mediaServiceData.take(service);

            if (d.plugin != 0)
                d.plugin->release(service);
        }
    }

    QMediaServiceProviderHint::Features supportedFeatures(const QMediaService *service) const
    {
        if (service) {
            MediaServiceData d = mediaServiceData.value(service);

            if (d.plugin) {
                QMediaServiceFeaturesInterface *iface =
                        qobject_cast<QMediaServiceFeaturesInterface*>(d.plugin);

                if (iface)
                    return iface->supportedFeatures(d.type);
            }
        }

        return QMediaServiceProviderHint::Features();
    }

    QMultimedia::SupportEstimate hasSupport(const QByteArray &serviceType,
                                     const QString &mimeType,
                                     const QStringList& codecs,
                                     int flags) const
    {
        const QList<QObject*> instances = loader()->instances(QLatin1String(serviceType));

        if (instances.isEmpty())
            return QMultimedia::NotSupported;

        bool allServicesProvideInterface = true;
        QMultimedia::SupportEstimate supportEstimate = QMultimedia::NotSupported;

        for (QObject *obj : instances) {
            QMediaServiceSupportedFormatsInterface *iface =
                    qobject_cast<QMediaServiceSupportedFormatsInterface*>(obj);


            if (flags) {
                QMediaServiceFeaturesInterface *iface =
                        qobject_cast<QMediaServiceFeaturesInterface*>(obj);

                if (iface) {
                    QMediaServiceProviderHint::Features features = iface->supportedFeatures(serviceType);

                    //if low latency playback was asked, skip services known
                    //not to provide low latency playback
                    if ((flags & QMediaPlayer::LowLatency) &&
                        !(features & QMediaServiceProviderHint::LowLatencyPlayback))
                            continue;

                    //the same for QIODevice based streams support
                    if ((flags & QMediaPlayer::StreamPlayback) &&
                        !(features & QMediaServiceProviderHint::StreamPlayback))
                            continue;
                }
            }

            if (iface)
                supportEstimate = qMax(supportEstimate, iface->hasSupport(mimeType, codecs));
            else
                allServicesProvideInterface = false;
        }

        //don't return PreferredService
        supportEstimate = qMin(supportEstimate, QMultimedia::ProbablySupported);

        //Return NotSupported only if no services are available of serviceType
        //or all the services returned NotSupported, otherwise return at least MaybeSupported
        if (!allServicesProvideInterface)
            supportEstimate = qMax(QMultimedia::MaybeSupported, supportEstimate);

        return supportEstimate;
    }

    QStringList supportedMimeTypes(const QByteArray &serviceType, int flags) const
    {
        const QList<QObject*> instances = loader()->instances(QLatin1String(serviceType));

        QStringList supportedTypes;

        for (QObject *obj : instances) {
            QMediaServiceSupportedFormatsInterface *iface =
                    qobject_cast<QMediaServiceSupportedFormatsInterface*>(obj);


            if (flags) {
                QMediaServiceFeaturesInterface *iface =
                        qobject_cast<QMediaServiceFeaturesInterface*>(obj);

                if (iface) {
                    QMediaServiceProviderHint::Features features = iface->supportedFeatures(serviceType);

                    // If low latency playback was asked for, skip MIME types from services known
                    // not to provide low latency playback
                    if ((flags & QMediaPlayer::LowLatency) &&
                        !(features & QMediaServiceProviderHint::LowLatencyPlayback))
                        continue;

                    //the same for QIODevice based streams support
                    if ((flags & QMediaPlayer::StreamPlayback) &&
                        !(features & QMediaServiceProviderHint::StreamPlayback))
                            continue;

                    //the same for QAbstractVideoSurface support
                    if ((flags & QMediaPlayer::VideoSurface) &&
                        !(features & QMediaServiceProviderHint::VideoSurface))
                            continue;
                }
            }

            if (iface) {
                supportedTypes << iface->supportedMimeTypes();
            }
        }

        // Multiple services may support the same MIME type
        supportedTypes.removeDuplicates();

        return supportedTypes;
    }

    QByteArray defaultDevice(const QByteArray &serviceType) const
    {
        const auto instances = loader()->instances(QLatin1String(serviceType));
        for (QObject *obj : instances) {
            const QMediaServiceDefaultDeviceInterface *iface =
                    qobject_cast<QMediaServiceDefaultDeviceInterface*>(obj);

            if (iface)
                return iface->defaultDevice(serviceType);
        }

        // if QMediaServiceDefaultDeviceInterface is not implemented, return the
        // first available device.
        QList<QByteArray> devs = devices(serviceType);
        if (!devs.isEmpty())
            return devs.first();

        return QByteArray();
    }

    QList<QByteArray> devices(const QByteArray &serviceType) const
    {
        QList<QByteArray> res;

        const auto instances = loader()->instances(QLatin1String(serviceType));
        for (QObject *obj : instances) {
            QMediaServiceSupportedDevicesInterface *iface =
                    qobject_cast<QMediaServiceSupportedDevicesInterface*>(obj);

            if (iface) {
                res.append(iface->devices(serviceType));
            }
        }

        return res;
    }

    QString deviceDescription(const QByteArray &serviceType, const QByteArray &device)
    {
        const auto instances = loader()->instances(QLatin1String(serviceType));
        for (QObject *obj : instances) {
            QMediaServiceSupportedDevicesInterface *iface =
                    qobject_cast<QMediaServiceSupportedDevicesInterface*>(obj);

            if (iface) {
                if (iface->devices(serviceType).contains(device))
                    return iface->deviceDescription(serviceType, device);
            }
        }

        return QString();
    }

    QCamera::Position cameraPosition(const QByteArray &device) const
    {
        const QByteArray serviceType(Q_MEDIASERVICE_CAMERA);
        const auto instances = loader()->instances(QString::fromLatin1(serviceType));
        for (QObject *obj : instances) {
            const QMediaServiceSupportedDevicesInterface *deviceIface =
                    qobject_cast<QMediaServiceSupportedDevicesInterface*>(obj);
            const QMediaServiceCameraInfoInterface *cameraIface =
                    qobject_cast<QMediaServiceCameraInfoInterface*>(obj);

            if (cameraIface) {
                if (deviceIface && !deviceIface->devices(serviceType).contains(device))
                    continue;
                return cameraIface->cameraPosition(device);
            }
        }

        return QCamera::UnspecifiedPosition;
    }

    int cameraOrientation(const QByteArray &device) const
    {
        const QByteArray serviceType(Q_MEDIASERVICE_CAMERA);
        const auto instances = loader()->instances(QString::fromLatin1(serviceType));
        for (QObject *obj : instances) {
            const QMediaServiceSupportedDevicesInterface *deviceIface =
                    qobject_cast<QMediaServiceSupportedDevicesInterface*>(obj);
            const QMediaServiceCameraInfoInterface *cameraIface =
                    qobject_cast<QMediaServiceCameraInfoInterface*>(obj);

            if (cameraIface) {
                if (deviceIface && !deviceIface->devices(serviceType).contains(device))
                    continue;
                return cameraIface->cameraOrientation(device);
            }
        }

        return 0;
    }
};

Q_GLOBAL_STATIC(QPluginServiceProvider, pluginProvider);

/*!
    \class QMediaServiceProvider
    \ingroup multimedia
    \ingroup multimedia_control
    \ingroup multimedia_core

    \internal

    \brief The QMediaServiceProvider class provides an abstract allocator for media services.
*/

/*!
    \fn QMediaServiceProvider::requestService(const QByteArray &type, const QMediaServiceProviderHint &hint)

    Requests an instance of a \a type service which best matches the given \a
    hint.

    Returns a pointer to the requested service, or a null pointer if there is
    no suitable service.

    The returned service must be released with releaseService when it is
    finished with.
*/

/*!
    \fn QMediaServiceProvider::releaseService(QMediaService *service)

    Releases a media \a service requested with requestService().
*/

/*!
    \fn QMediaServiceProvider::supportedFeatures(const QMediaService *service) const

    Returns the features supported by a given \a service.
*/
QMediaServiceProviderHint::Features QMediaServiceProvider::supportedFeatures(const QMediaService *service) const
{
    Q_UNUSED(service);

    return QMediaServiceProviderHint::Features(0);
}

/*!
    \fn QMultimedia::SupportEstimate QMediaServiceProvider::hasSupport(const QByteArray &serviceType, const QString &mimeType, const QStringList& codecs, int flags) const

    Returns how confident a media service provider is that is can provide a \a
    serviceType service that is able to play media of a specific \a mimeType
    that is encoded using the listed \a codecs while adhering to constraints
    identified in \a flags.
*/
QMultimedia::SupportEstimate QMediaServiceProvider::hasSupport(const QByteArray &serviceType,
                                                        const QString &mimeType,
                                                        const QStringList& codecs,
                                                        int flags) const
{
    Q_UNUSED(serviceType);
    Q_UNUSED(mimeType);
    Q_UNUSED(codecs);
    Q_UNUSED(flags);

    return QMultimedia::MaybeSupported;
}

/*!
    \fn QStringList QMediaServiceProvider::supportedMimeTypes(const QByteArray &serviceType, int flags) const

    Returns a list of MIME types supported by the service provider for the
    specified \a serviceType.

    The resultant list is restricted to MIME types which can be supported given
    the constraints in \a flags.
*/
QStringList QMediaServiceProvider::supportedMimeTypes(const QByteArray &serviceType, int flags) const
{
    Q_UNUSED(serviceType);
    Q_UNUSED(flags);

    return QStringList();
}

/*!
  \since 5.3

  Returns the default device for a \a service type.
*/
QByteArray QMediaServiceProvider::defaultDevice(const QByteArray &serviceType) const
{
    Q_UNUSED(serviceType);
    return QByteArray();
}

/*!
  Returns the list of devices related to \a service type.
*/
QList<QByteArray> QMediaServiceProvider::devices(const QByteArray &service) const
{
    Q_UNUSED(service);
    return QList<QByteArray>();
}

/*!
    Returns the description of \a device related to \a serviceType, suitable for use by
    an application for display.
*/
QString QMediaServiceProvider::deviceDescription(const QByteArray &serviceType, const QByteArray &device)
{
    Q_UNUSED(serviceType);
    Q_UNUSED(device);
    return QString();
}

/*!
    \since 5.3

    Returns the physical position of a camera \a device on the system hardware.
*/
QCamera::Position QMediaServiceProvider::cameraPosition(const QByteArray &device) const
{
    Q_UNUSED(device);
    return QCamera::UnspecifiedPosition;
}

/*!
    \since 5.3

    Returns the physical orientation of the camera \a device. The value is the angle by which the
    camera image should be rotated anti-clockwise (in steps of 90 degrees) so it shows correctly on
    the display in its natural orientation.
*/
int QMediaServiceProvider::cameraOrientation(const QByteArray &device) const
{
    Q_UNUSED(device);
    return 0;
}

static QMediaServiceProvider *qt_defaultMediaServiceProvider = 0;

/*!
    Sets a media service \a provider as the default.
    It's useful for unit tests to provide mock service.

    \internal
*/
void QMediaServiceProvider::setDefaultServiceProvider(QMediaServiceProvider *provider)
{
    qt_defaultMediaServiceProvider = provider;
}


/*!
    Returns a default provider of media services.
*/
QMediaServiceProvider *QMediaServiceProvider::defaultServiceProvider()
{
    return qt_defaultMediaServiceProvider != 0
            ? qt_defaultMediaServiceProvider
            : static_cast<QMediaServiceProvider *>(pluginProvider());
}

/*!
    \class QMediaServiceProviderPlugin
    \inmodule QtMultimedia
    \brief The QMediaServiceProviderPlugin class interface provides an interface for QMediaService
    plug-ins.

    A media service provider plug-in may implement one or more of
    QMediaServiceSupportedFormatsInterface,
    QMediaServiceSupportedDevicesInterface, and QMediaServiceFeaturesInterface
    to identify the features it supports.
*/

/*!
    \fn QMediaServiceProviderPlugin::create(const QString &key)

    Constructs a new instance of the QMediaService identified by \a key.

    The QMediaService returned must be destroyed with release().
*/

/*!
    \fn QMediaServiceProviderPlugin::release(QMediaService *service)

    Destroys a media \a service constructed with create().
*/


/*!
    \class QMediaServiceSupportedFormatsInterface
    \inmodule QtMultimedia
    \brief The QMediaServiceSupportedFormatsInterface class interface
    identifies if a media service plug-in supports a media format.

    A QMediaServiceProviderPlugin may implement this interface.
*/

/*!
    \fn QMediaServiceSupportedFormatsInterface::~QMediaServiceSupportedFormatsInterface()

    Destroys a media service supported formats interface.
*/

/*!
    \fn QMediaServiceSupportedFormatsInterface::hasSupport(const QString &mimeType, const QStringList& codecs) const

    Returns the level of support a media service plug-in has for a \a mimeType
    and set of \a codecs.
*/

/*!
    \fn QMediaServiceSupportedFormatsInterface::supportedMimeTypes() const

    Returns a list of MIME types supported by the media service plug-in.
*/

/*!
    \class QMediaServiceSupportedDevicesInterface
    \inmodule QtMultimedia
    \brief The QMediaServiceSupportedDevicesInterface class interface
    identifies the devices supported by a media service plug-in.

    A QMediaServiceProviderPlugin may implement this interface.
*/

/*!
    \fn QMediaServiceSupportedDevicesInterface::~QMediaServiceSupportedDevicesInterface()

    Destroys a media service supported devices interface.
*/

/*!
    \fn QList<QByteArray> QMediaServiceSupportedDevicesInterface::devices(const QByteArray &service) const

    Returns a list of devices available for a \a service type.
*/

/*!
    \fn QString QMediaServiceSupportedDevicesInterface::deviceDescription(const QByteArray &service, const QByteArray &device)

    Returns the description of a \a device available for a \a service type.
*/

/*!
    \class QMediaServiceDefaultDeviceInterface
    \inmodule QtMultimedia
    \brief The QMediaServiceDefaultDeviceInterface class interface
    identifies the default device used by a media service plug-in.

    A QMediaServiceProviderPlugin may implement this interface.

    \since 5.3
*/

/*!
    \fn QMediaServiceDefaultDeviceInterface::~QMediaServiceDefaultDeviceInterface()

    Destroys a media service default device interface.
*/

/*!
    \fn QByteArray QMediaServiceDefaultDeviceInterface::defaultDevice(const QByteArray &service) const

    Returns the default device for a \a service type.
*/

/*!
    \class QMediaServiceCameraInfoInterface
    \inmodule QtMultimedia
    \since 5.3
    \brief The QMediaServiceCameraInfoInterface class interface
    provides camera-specific information about devices supported by a camera service plug-in.

    A QMediaServiceProviderPlugin may implement this interface, in that case it also needs to
    implement the QMediaServiceSupportedDevicesInterface.
*/

/*!
    \fn QMediaServiceCameraInfoInterface::~QMediaServiceCameraInfoInterface()

    Destroys a media service camera info interface.
*/

/*!
    \fn QMediaServiceCameraInfoInterface::cameraPosition(const QByteArray &device) const

    Returns the physical position of a camera \a device supported by a camera service plug-in.
*/

/*!
    \fn QMediaServiceCameraInfoInterface::cameraOrientation(const QByteArray &device) const

    Returns the physical orientation of a camera \a device supported by a camera service plug-in.
*/

/*!
    \class QMediaServiceFeaturesInterface
    \inmodule QtMultimedia
    \brief The QMediaServiceFeaturesInterface class interface identifies
    features supported by a media service plug-in.

    A QMediaServiceProviderPlugin may implement this interface.
*/

/*!
    \fn QMediaServiceFeaturesInterface::~QMediaServiceFeaturesInterface()

    Destroys a media service features interface.
*/
/*!
    \fn QMediaServiceFeaturesInterface::supportedFeatures(const QByteArray &service) const

    Returns a set of features supported by a plug-in \a service.
*/

#include "moc_qmediaserviceprovider_p.cpp"
#include "moc_qmediaserviceproviderplugin.cpp"
QT_END_NAMESPACE

