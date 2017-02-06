/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>

#include <gruesensor.h>
#include <QDebug>

#ifdef BUNDLED_PLUGIN
#include <QPluginLoader>
#include <QSensorPluginInterface>
#endif

QT_BEGIN_NAMESPACE

class GrueSensorQmlImport : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid FILE "import.json")
public:
    virtual void registerTypes(const char *uri)
    {
        char const * const package = "Grue";
        if (QLatin1String(uri) != QLatin1String(package)) return;
        int major;
        int minor;

        // Register the 1.0 interfaces
        major = 1;
        minor = 0;
        // @uri Grue
        qmlRegisterType           <GrueSensor       >(package, major, minor, "GrueSensor");
        qmlRegisterUncreatableType<GrueSensorReading>(package, major, minor, "GrueSensorReading", QLatin1String("Cannot create GrueSensorReading"));
    }

#ifdef BUNDLED_PLUGIN
    GrueSensorQmlImport()
    {
        // For now, this is getting called after Sensors has loaded
        // Ensure that a change later does not break this by forcing
        // sensors to load now
        (void)QSensor::sensorTypes();

        // Load the bundled sensor plugin
        QPluginLoader loader(QString::fromLocal8Bit(BUNDLED_PLUGIN));
        QObject *instance = loader.instance();
        m_changes = qobject_cast<QSensorChangesInterface*>(instance);
        if (m_changes) {
            QSensor *sensor = new QSensor(QByteArray(), this);
            connect(sensor, SIGNAL(availableSensorsChanged()), this, SLOT(sensorsChanged()));
            m_changes->sensorsChanged();
        }
        QSensorPluginInterface *plugin = qobject_cast<QSensorPluginInterface*>(instance);
        if (plugin) {
            plugin->registerSensors();
        }
    }

private slots:
    void sensorsChanged()
    {
        m_changes->sensorsChanged();
    }

private:
    QSensorChangesInterface *m_changes;
#endif
};

QT_END_NAMESPACE

#include "main.moc"

/*
    \omit
    \qmltype GrueSensor
    \instantiates GrueSensor
    \inherits Sensor
    \inqmlmodule Grue
    \brief The GrueSensor type reports on your chance of being eaten by a Grue.

    The GrueSensor type reports on your chance of being eaten by a Grue.

    This type wraps the GrueSensor class. Please see the documentation for
    GrueSensor for details.
    \endomit
*/

/*
    \omit
    \qmltype GrueSensorReading
    \instantiates GrueSensorReading
    \inherits SensorReading
    \inqmlmodule Grue
    \brief The GrueSensorReading type holds the most recent GrueSensor reading.

    The GrueSensorReading type holds the most recent GrueSensor reading.

    This type wraps the GrueSensorReading class. Please see the documentation for
    GrueSensorReading for details.

    This type cannot be directly created.
    \endomit
*/

/*
    \omit
    \qmlproperty qreal Grue1::GrueSensorReading::chanceOfBeingEaten
    Please see GrueSensorReading::chanceOfBeingEaten for information about this property.
    \endomit
*/
