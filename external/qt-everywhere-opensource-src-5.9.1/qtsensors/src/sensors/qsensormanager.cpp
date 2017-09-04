/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "qsensormanager.h"
#include <QDebug>
#include <private/qfactoryloader_p.h>
#include <QPluginLoader>
#include "qsensorplugin.h"
#include <QStandardPaths>
#include "sensorlog_p.h"
#include <QTimer>
#include <QFile>
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

typedef QHash<QByteArray,QSensorBackendFactory*> FactoryForIdentifierMap;
typedef QHash<QByteArray,FactoryForIdentifierMap> BackendIdentifiersForTypeMap;

static QLoggingCategory sensorsCategory("qt.sensors");

class QSensorManagerPrivate : public QObject
{
    friend class QSensorManager;

    Q_OBJECT
public:
    enum PluginLoadingState {
        NotLoaded,
        Loading,
        Loaded
    };
    QSensorManagerPrivate()
        : loadExternalPlugins(true)
        , pluginLoadingState(NotLoaded)
        , loader(new QFactoryLoader("com.qt-project.Qt.QSensorPluginInterface/1.0", QLatin1String("/sensors")))
        , defaultIdentifierForTypeLoaded(false)
        , sensorsChanged(false)
    {
        QByteArray env = qgetenv("QT_SENSORS_LOAD_PLUGINS");
        if (env == "0") {
            loadExternalPlugins = false;
        }
    }
    bool loadExternalPlugins;
    PluginLoadingState pluginLoadingState;
    QFactoryLoader *loader;
    void loadPlugins();

    // Holds a mapping from type to available identifiers (and from there to the factory)
    BackendIdentifiersForTypeMap backendsByType;

    // Holds the default identifier
    QHash<QByteArray, QByteArray> defaultIdentifierForType;
    bool defaultIdentifierForTypeLoaded;
    void readConfigFile()
    {
        defaultIdentifierForTypeLoaded = true;
#ifdef QTSENSORS_CONFIG_PATH
        QString config = QString::fromLocal8Bit(QTSENSORS_CONFIG_PATH);
#else
        QStringList configs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
        if (configs.count() == 0) return; // QStandardPaths is broken?
        QString config = configs.at(configs.count()-1);
        if (config.isEmpty()) return; // QStandardPaths is broken?
        config += QLatin1String("/QtProject/Sensors.conf");
#endif
        qCDebug(sensorsCategory) << "Loading config from" << config;
        if (!QFile::exists(config)) {
            qCDebug(sensorsCategory) << "There is no config file" << config;
            return;
        }
        QFile cfgfile(config);
        if (!cfgfile.open(QFile::ReadOnly)) {
            qCWarning(sensorsCategory) << "Can't open config file" << config;
            return;
        }

        QTextStream stream(&cfgfile);
        QString line;
        bool isconfig = false;
        while (!stream.atEnd()) {
            line = stream.readLine();
            if (!isconfig && line == QLatin1String("[Default]"))
                isconfig = true;
            else if (isconfig) {
                //read out setting line
                line.remove(' ');
                QStringList pair = line.split('=');
                if (pair.count() == 2)
                    defaultIdentifierForType.insert(pair[0].toLatin1(), pair[1].toLatin1());
            }
        }
    }

    // Holds the first identifier for each type
    QHash<QByteArray, QByteArray> firstIdentifierForType;

    bool sensorsChanged;
    QList<QSensorChangesInterface*> changeListeners;
    QSet <QObject *> seenPlugins;

Q_SIGNALS:
    void availableSensorsChanged();

public Q_SLOTS:
    void emitSensorsChanged()
    {
        static bool alreadyRunning = false;
        if (pluginLoadingState != QSensorManagerPrivate::Loaded || alreadyRunning) {
            // We're busy.
            // Just note that a registration changed and exit.
            // Someone up the call stack will deal with this.
            sensorsChanged = true;
            return;
        }

        // Set a flag so any recursive calls doesn't cause a loop.
        alreadyRunning = true;

        // Since one [un]registration may cause other [un]registrations and since
        // the order in which we do things matters we just do a cascading update
        // until things stop changing.
        do {
            sensorsChanged = false;
            Q_FOREACH (QSensorChangesInterface *changes, changeListeners) {
                changes->sensorsChanged();
            }
        } while (sensorsChanged);

        // We're going away now so clear the flag
        alreadyRunning = false;

        // Notify the client of the changes
        Q_EMIT availableSensorsChanged();
    }
};

Q_GLOBAL_STATIC(QSensorManagerPrivate, sensorManagerPrivate)

static void initPlugin(QObject *o, bool warnOnFail = true)
{
    qCDebug(sensorsCategory) << "Init plugin" << o;
    if (!o) {
        qCWarning(sensorsCategory) << "Null plugin" << o;
        return;
    }

    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return; // hardly likely but just in case...

    if (d->seenPlugins.contains(o)) {
        qCDebug(sensorsCategory) << "Plugin is seen" << o;
        return;
    }

    QSensorChangesInterface *changes = qobject_cast<QSensorChangesInterface*>(o);
    if (changes)
        d->changeListeners << changes;

    QSensorPluginInterface *plugin = qobject_cast<QSensorPluginInterface*>(o);

    if (plugin) {
        qCDebug(sensorsCategory) << "Register sensors for " << plugin;
        d->seenPlugins.insert(o);
        plugin->registerSensors();
    } else if (warnOnFail) {
        qCWarning(sensorsCategory) << "Can't cast to plugin" << o;
    }
}

void QSensorManagerPrivate::loadPlugins()
{
    QSensorManagerPrivate *d = this;
    if (d->pluginLoadingState != QSensorManagerPrivate::NotLoaded) return;
    d->pluginLoadingState = QSensorManagerPrivate::Loading;

    SENSORLOG() << "initializing static plugins";
    // Qt-style static plugins
    Q_FOREACH (QObject *plugin, QPluginLoader::staticInstances()) {
        initPlugin(plugin, false/*do not warn on fail*/);
    }

    if (d->loadExternalPlugins) {
        SENSORLOG() << "initializing plugins";
        QList<QJsonObject> meta = d->loader->metaData();
        for (int i = 0; i < meta.count(); i++) {
            QObject *plugin = d->loader->instance(i);
            initPlugin(plugin);
        }
    }

    d->pluginLoadingState = QSensorManagerPrivate::Loaded;

    if (d->sensorsChanged) {
        // Notify the app that the available sensor list has changed.
        // This may cause recursive calls!
        d->emitSensorsChanged();
    }
}

// =====================================================================

/*!
    \class QSensorManager
    \ingroup sensors_backend
    \inmodule QtSensors

    \brief The QSensorManager class handles registration and creation of sensor backends.

    Sensor plugins register backends using the registerBackend() function.

    When QSensor::connectToBackend() is called, the createBackend() function will be called.
*/

/*!
    Register a sensor for \a type. The \a identifier must be unique.

    The \a factory will be asked to create instances of the backend.
*/
void QSensorManager::registerBackend(const QByteArray &type, const QByteArray &identifier, QSensorBackendFactory *factory)
{
    Q_ASSERT(type.count());
    Q_ASSERT(identifier.count());
    Q_ASSERT(factory);
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return; // hardly likely but just in case...
    if (!d->backendsByType.contains(type)) {
        (void)d->backendsByType[type];
        d->firstIdentifierForType[type] = identifier;
    } else if (d->firstIdentifierForType[type].startsWith("generic.")) {
        // Don't let a generic backend be the default when some other backend exists!
        d->firstIdentifierForType[type] = identifier;
    }
    FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[type];
    if (factoryByIdentifier.contains(identifier)) {
        qWarning() << "A backend with type" << type << "and identifier" << identifier << "has already been registered!";
        return;
    }
    SENSORLOG() << "registering backend for type" << type << "identifier" << identifier;// << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
    factoryByIdentifier[identifier] = factory;

    // Notify the app that the available sensor list has changed.
    // This may cause recursive calls!
    d->emitSensorsChanged();
}

/*!
    Unregister the backend for \a type with \a identifier.

    Note that this only prevents new instance of the backend from being created. It does not
    invalidate the existing instances of the backend. The backend code should handle the
    disappearance of the underlying hardware itself.
*/
void QSensorManager::unregisterBackend(const QByteArray &type, const QByteArray &identifier)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return; // hardly likely but just in case...
    if (!d->backendsByType.contains(type)) {
        qWarning() << "No backends of type" << type << "are registered";
        return;
    }
    FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[type];
    if (!factoryByIdentifier.contains(identifier)) {
        qWarning() << "Identifier" << identifier << "is not registered";
        return;
    }

    (void)factoryByIdentifier.take(identifier); // we don't own this pointer anyway
    if (d->firstIdentifierForType[type] == identifier) {
        if (factoryByIdentifier.count()) {
            d->firstIdentifierForType[type] = factoryByIdentifier.begin().key();
            if (d->firstIdentifierForType[type].startsWith("generic.")) {
                // Don't let a generic backend be the default when some other backend exists!
                for (FactoryForIdentifierMap::const_iterator it = factoryByIdentifier.begin()++; it != factoryByIdentifier.end(); ++it) {
                    const QByteArray &identifier(it.key());
                    if (!identifier.startsWith("generic.")) {
                        d->firstIdentifierForType[type] = identifier;
                        break;
                    }
                }
            }
        } else {
            (void)d->firstIdentifierForType.take(type);
        }
    }
    if (!factoryByIdentifier.count())
        (void)d->backendsByType.take(type);

    // Notify the app that the available sensor list has changed.
    // This may cause recursive calls!
    d->emitSensorsChanged();
}

/*!
    Create a backend for \a sensor. Returns null if no suitable backend exists.
*/
QSensorBackend *QSensorManager::createBackend(QSensor *sensor)
{
    Q_ASSERT(sensor);

    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return 0; // hardly likely but just in case...
    d->loadPlugins();

    SENSORLOG() << "QSensorManager::createBackend" << "type" << sensor->type() << "identifier" << sensor->identifier();

    if (!d->backendsByType.contains(sensor->type())) {
        SENSORLOG() << "no backends of type" << sensor->type() << "have been registered.";
        return 0;
    }

    const FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[sensor->type()];
    QSensorBackendFactory *factory;
    QSensorBackend *backend;

    if (sensor->identifier().isEmpty()) {
        QByteArray defaultIdentifier = QSensor::defaultSensorForType(sensor->type());
        SENSORLOG() << "Trying the default" << defaultIdentifier;
        // No identifier set, try the default
        factory = factoryByIdentifier[defaultIdentifier];
        //SENSORLOG() << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
        sensor->setIdentifier(defaultIdentifier); // the factory requires this
        backend = factory->createBackend(sensor);
        if (backend) return backend; // Got it!

        // The default failed to instantiate so try any other registered sensors for this type
        Q_FOREACH (const QByteArray &identifier, factoryByIdentifier.keys()) {
            SENSORLOG() << "Trying" << identifier;
            if (identifier == defaultIdentifier) continue; // Don't do the default one again
            factory = factoryByIdentifier[identifier];
            //SENSORLOG() << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
            sensor->setIdentifier(identifier); // the factory requires this
            backend = factory->createBackend(sensor);
            if (backend) return backend; // Got it!
        }
        SENSORLOG() << "FAILED";
        sensor->setIdentifier(QByteArray()); // clear the identifier
    } else {
        if (!factoryByIdentifier.contains(sensor->identifier())) {
            SENSORLOG() << "no backend with identifier" << sensor->identifier() << "for type" << sensor->type();
            return 0;
        }

        // We were given an explicit identifier so don't substitute other backends if it fails to instantiate
        factory = factoryByIdentifier[sensor->identifier()];
        //SENSORLOG() << "factory" << QString().sprintf("0x%08x", (unsigned int)factory);
        backend = factory->createBackend(sensor);
        if (backend) return backend; // Got it!
    }

    SENSORLOG() << "no suitable backend found for requested identifier" << sensor->identifier() << "and type" << sensor->type();
    return 0;
}

/*!
    Returns true if the backend identified by \a type and \a identifier is registered.

    This is a convenience method that helps out plugins doing dynamic registration.
*/
bool QSensorManager::isBackendRegistered(const QByteArray &type, const QByteArray &identifier)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return false; // hardly likely but just in case...
    d->loadPlugins();

    if (!d->backendsByType.contains(type))
        return false;

    const FactoryForIdentifierMap &factoryByIdentifier = d->backendsByType[type];
    if (!factoryByIdentifier.contains(identifier))
        return false;

    return true;
}

/*!
    Sets or overwrite the sensor \a type with the backend \a identifier.
*/
void QSensorManager::setDefaultBackend(const QByteArray &type, const QByteArray &identifier)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return; // hardly likely but just in case...
    d->defaultIdentifierForType.insert(type, identifier);
}


// =====================================================================

/*!
    Returns a list of all sensor types.
*/
QList<QByteArray> QSensor::sensorTypes()
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return QList<QByteArray>(); // hardly likely but just in case...
    d->loadPlugins();

    return d->backendsByType.keys();
}

/*!
    Returns a list of ids for each of the sensors for \a type.
    If there are no sensors of that type available the list will be empty.
*/
QList<QByteArray> QSensor::sensorsForType(const QByteArray &type)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return QList<QByteArray>(); // hardly likely but just in case...
    d->loadPlugins();

    // no sensors of that type exist
    if (!d->backendsByType.contains(type))
        return QList<QByteArray>();

    return d->backendsByType[type].keys();
}

/*!
    Returns the default sensor identifier for \a type.
    This is set in a config file and can be overridden if required.
    If no default is available the system will return the first registered
    sensor for \a type.

    Note that there is special case logic to prevent the generic plugin's backends from becoming the
    default when another backend is registered for the same type. This logic means that a backend
    identifier starting with \c{generic.} will only be the default if no other backends have been
    registered for that type or if it is specified in \c{Sensors.conf}.

    \sa {Determining the default sensor for a type}
*/
QByteArray QSensor::defaultSensorForType(const QByteArray &type)
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return QByteArray(); // hardly likely but just in case...
    d->loadPlugins();

    // no sensors of that type exist
    if (!d->backendsByType.contains(type))
        return QByteArray();

    //check if we need to read the config setting file
    if (!d->defaultIdentifierForTypeLoaded)
        d->readConfigFile();

    QHash<QByteArray, QByteArray>::const_iterator i = d->defaultIdentifierForType.find(type);
    if (i != d->defaultIdentifierForType.end() && i.key() == type) {
        if (d->backendsByType[type].contains(i.value())) // Don't return a value that we can't use!
            return i.value();
    }

    // This is our fallback
    return d->firstIdentifierForType[type];
}

void QSensor::registerInstance()
{
    QSensorManagerPrivate *d = sensorManagerPrivate();
    if (!d) return; // hardly likely but just in case...
    connect(d, SIGNAL(availableSensorsChanged()), this, SIGNAL(availableSensorsChanged()));
}

// =====================================================================

/*!
    \class QSensorBackendFactory
    \ingroup sensors_backend
    \inmodule QtSensors

    \brief The QSensorBackendFactory class instantiates instances of
           QSensorBackend.

    This interface must be implemented in order to register a sensor backend.

    \sa {Creating a sensor plugin}
*/

/*!
    \internal
*/
QSensorBackendFactory::~QSensorBackendFactory()
{
}

/*!
    \fn QSensorBackendFactory::createBackend(QSensor *sensor)

    Instantiate a backend. If the factory handles multiple identifiers
    it should check with the \a sensor to see which one is requested.

    If the factory cannot create a backend it should return 0.
*/

QT_END_NAMESPACE

#include "qsensormanager.moc"
