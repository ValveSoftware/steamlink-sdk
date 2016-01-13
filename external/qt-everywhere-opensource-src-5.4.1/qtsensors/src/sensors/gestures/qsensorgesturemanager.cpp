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

#include "qsensorgesturemanager.h"
#include "qsensorgesturemanagerprivate_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QSensorGestureManager
    \ingroup sensorgestures_main
    \inmodule QtSensors
    \since 5.1

    \brief The QSensorGestureManager class manages sensor gestures, registers and creates sensor gesture plugins.

    Sensor Gesture plugins register their recognizers using the registerSensorGestureRecognizer() function.

    \snippet sensorgestures/creating.cpp Receiving sensor gesture signals
*/

/*!
  \fn QSensorGestureManager::newSensorGestureAvailable()
  Signals when a new sensor gesture becomes available for use.
  */

/*!
  Constructs the QSensorGestureManager as a child of \a parent
  */
QSensorGestureManager::QSensorGestureManager(QObject *parent)
    : QObject(parent)
{
    QSensorGestureManagerPrivate *d = QSensorGestureManagerPrivate::instance();
    if (!d) return; // hardly likely but just in case...
    connect(d,SIGNAL(newSensorGestureAvailable()),
            this,SIGNAL(newSensorGestureAvailable()));
}

/*!
    Destroy the QSensorGestureManager
*/
QSensorGestureManager::~QSensorGestureManager()
{
}

/*!
  Registers the sensor recognizer \a recognizer for use.
  QSensorGestureManager retains ownership of the recognizer object.
  Returns true unless the gesture has already been registered, in
  which case the object is deleted.

  */

 bool QSensorGestureManager::registerSensorGestureRecognizer(QSensorGestureRecognizer *recognizer)
 {
     QSensorGestureManagerPrivate *d = QSensorGestureManagerPrivate::instance();
     if (!d) { // hardly likely but just in case...
         delete recognizer;
         return false;
     }
     bool ok = d->registerSensorGestureRecognizer(recognizer);
     if (!ok)
         delete recognizer;

     return ok;
 }


 /*!
   Returns the list of the currently registered gestures.
   Includes all the standard built-ins as well as available plugins.
   */
  QStringList QSensorGestureManager::gestureIds() const
 {
      QSensorGestureManagerPrivate *d = QSensorGestureManagerPrivate::instance();
      if (!d) return QStringList(); // hardly likely but just in case...
      return d->gestureIds();
 }

  /*!
    Returns the list of all the gesture signals for the registered \a gestureId gesture recognizer id.
    */
  QStringList QSensorGestureManager::recognizerSignals(const QString &gestureId) const
  {
      QSensorGestureRecognizer *recognizer = sensorGestureRecognizer(gestureId);
      if (recognizer != 0)
          return recognizer->gestureSignals();
      else
          return QStringList();
  }

/*!
  Returns the sensor gesture object for the recognizer \a id.
  */
QSensorGestureRecognizer *QSensorGestureManager::sensorGestureRecognizer(const QString &id)
{
    QSensorGestureManagerPrivate *d = QSensorGestureManagerPrivate::instance();
    if (!d) return 0; // hardly likely but just in case...
    return d->sensorGestureRecognizer(id);
}

QT_END_NAMESPACE
