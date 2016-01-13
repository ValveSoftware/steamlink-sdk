/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <QDir>
#include <QLibraryInfo>

#include <QtCore/private/qfactoryloader_p.h>

#include "qsensorgesturerecognizer.h"
#include "qsensorgesturemanagerprivate_p.h"
#include "qsensorgestureplugininterface.h"

#ifdef SIMULATOR_BUILD
#include "simulatorgesturescommon_p.h"
#endif

Q_GLOBAL_STATIC(QSensorGestureManagerPrivate, sensorGestureManagerPrivate)

QT_BEGIN_NAMESPACE

QSensorGestureManagerPrivate::QSensorGestureManagerPrivate(QObject *parent) :
    QObject(parent)
{
#ifdef SIMULATOR_BUILD
    SensorGesturesConnection *connection =  new SensorGesturesConnection(this);
    connect(connection,SIGNAL(sensorGestureDetected()),
            this,SLOT(sensorGestureDetected()));

    connect(this,SIGNAL(newSensorGestures(QStringList)),
            connection,SLOT(newSensorGestures(QStringList)));

    connect(this,SIGNAL(removeSensorGestures(QStringList)),
            connection,SLOT(removeSensorGestures(QStringList)));
#endif

    loader = new QFactoryLoader("org.qt-project.QSensorGesturePluginInterface", QLatin1String("/sensorgestures"));
    loadPlugins();
}

QSensorGestureManagerPrivate::~QSensorGestureManagerPrivate()
{
//    qDeleteAll(registeredSensorGestures);
//    delete loader;
}


 void QSensorGestureManagerPrivate::initPlugin(QObject *plugin)
{
    if (QSensorGesturePluginInterface *pInterface
            = qobject_cast<QSensorGesturePluginInterface *>(plugin)) {

        Q_FOREACH (const QString &id, pInterface->supportedIds()) {

            if (!knownIds.contains(id))
                knownIds.append(id);
            else
                qWarning() << id <<"from the plugin"
                           << pInterface->name()
                           << "is already known.";

        }
        plugins << plugin;
    } else {
        qWarning() << "Could not load "<< plugin;
    }
}


/*!
  Internal
  Loads the sensorgesture plugins.
  */
void QSensorGestureManagerPrivate::loadPlugins()
{
    Q_FOREACH (QObject *plugin, QPluginLoader::staticInstances()) {
        initPlugin(plugin);
    }
    QList<QJsonObject> meta = loader->metaData();
    for (int i = 0; i < meta.count(); i++) {
        QObject *plugin = loader->instance(i);
        initPlugin(plugin);
    }
}


/*!
  Internal
  creates the requested  recognizer.
  */

bool QSensorGestureManagerPrivate::loadRecognizer(const QString &recognizerId)
{
    //if no plugin is used return true if this is a registered recognizer

    if (registeredSensorGestures.contains(recognizerId))
        return true;

    for (int i= 0; i < plugins.count(); i++) {

        if (QSensorGesturePluginInterface *pInterface
                = qobject_cast<QSensorGesturePluginInterface *>(plugins.at(i))) {

            if (pInterface->supportedIds().contains(recognizerId)) {

                if (!registeredSensorGestures.contains(recognizerId)) {
                    //create these recognizers
                    QList <QSensorGestureRecognizer *> recognizers = pInterface->createRecognizers();

                    Q_FOREACH (QSensorGestureRecognizer *recognizer, recognizers) {

                        if (registeredSensorGestures.contains(recognizer->id())) {
                           qWarning() << "Ignoring recognizer " << recognizer->id() << "from plugin" << pInterface->name() << "because it is already registered";
                            delete recognizer;
                        } else {
                            registeredSensorGestures.insert(recognizer->id(),recognizer);
                        }
                    }
                }
                return true;
            }
        }
    }
    return false;
}

bool QSensorGestureManagerPrivate::registerSensorGestureRecognizer(QSensorGestureRecognizer *recognizer)
{
    if (!knownIds.contains(recognizer->id())) {
        knownIds.append(recognizer->id());
        Q_ASSERT (!registeredSensorGestures.contains(recognizer->id()));
        recognizer->setParent(0);
        registeredSensorGestures.insert(recognizer->id(),recognizer);
        Q_EMIT newSensorGestureAvailable();

        return true;
    }
    return false;
}

QSensorGestureRecognizer *QSensorGestureManagerPrivate::sensorGestureRecognizer(const QString &id)
{
    QSensorGestureRecognizer *recognizer = 0;

    if (loadRecognizer(id)) {
        recognizer= registeredSensorGestures.value(id);
    }

    return recognizer;
}

QStringList QSensorGestureManagerPrivate::gestureIds()
{
    return knownIds;
}

#ifdef SIMULATOR_BUILD
void QSensorGestureManagerPrivate::sensorGestureDetected()
{
    QString str = get_qtSensorGestureData();

    Q_FOREACH (const QString &id, gestureIds()) {
        QSensorGestureRecognizer *recognizer = sensorGestureRecognizer(id);
        if (recognizer != 0) {
            Q_FOREACH (const QString &sig,  recognizer->gestureSignals()) {
                if (!sig.contains(QLatin1String("detected"))) { //weed out detected signals
                    QString tmp;
                        tmp = sig.left(sig.length() - 2);
                    if (str == tmp) {
                        // named signal for c++
                        QMetaObject::invokeMethod(recognizer, str.toLocal8Bit(), Qt::DirectConnection);
                        // detected signal for qml and c++
                        QMetaObject::invokeMethod(recognizer, "detected", Qt::DirectConnection,
                                                  Q_ARG(QString, str));
                        break;

                    }
                }
            }
        }
    }
}

void QSensorGestureManagerPrivate::recognizerStarted(const QSensorGestureRecognizer *recognizer)
{
    QStringList list = recognizer->gestureSignals();
    list.removeOne(QLatin1String("detected(QString)"));
    Q_EMIT newSensorGestures(list);
}

void QSensorGestureManagerPrivate::recognizerStopped(const QSensorGestureRecognizer *recognizer)
{
    QStringList list = recognizer->gestureSignals();
    list.removeOne(QLatin1String("detected(QString)"));
    Q_EMIT removeSensorGestures(list);
}

#endif

QSensorGestureManagerPrivate * QSensorGestureManagerPrivate::instance()
{
    QSensorGestureManagerPrivate *priv = sensorGestureManagerPrivate();
    // It's safe to return 0 because this is checked when used
    //if (!priv) qFatal("Cannot return from QSensorGestureManagerPrivate::instance() because sensorGestureManagerPrivate() returned 0.");
    return priv;
}


QT_END_NAMESPACE
