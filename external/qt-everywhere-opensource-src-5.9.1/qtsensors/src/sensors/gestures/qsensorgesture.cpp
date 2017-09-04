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

#include <QDir>
#include <QPluginLoader>
#include <QDebug>

#include "qsensorgesture.h"
#include "qsensorgesture_p.h"
#include "qsensorgesturemanager.h"

#include <private/qmetaobjectbuilder_p.h>

/*!
    \class QSensorGesture
    \ingroup sensorgestures_main
    \inmodule QtSensors
    \since 5.1

    \brief The QSensorGesture class represents one or more sensor gesture recognizers.

    In addition to the QSensorGesture::detected() signal, Sensor Gesture Recognizers can
    have their own specific signals, and may be discovered through
    QSensorGesture::gestureSignals().

    \b {Note that QSensorGesture uses a custom meta-object in order to provide
    recognizer-specific signals. This means it is not possible to sub-class
    QSensorGesture and use Q_OBJECT. Also qobject_cast<QSensorGesture*>(ptr) will
    not work.}

    \sa QSensorGestureRecognizer

    You may use QSensorGestureManager to obtain the systems known sensor gesture ids.

    \sa QSensorGestureManager
  */

#ifdef Q_QDOC
/*!
  \fn QSensorGesture::detected(QString gestureId)
  Signals when the \a gestureId gesture has been recognized.
  */
#endif

/*!
  Constructs the sensor gesture, and initializes the \a ids list of recognizers,
  with parent \a parent
  */
QSensorGesture::QSensorGesture(const QStringList &ids, QObject *parent) :
    QObject(parent)
{
    d_ptr = new QSensorGesturePrivate();
    Q_FOREACH (const QString &id, ids) {
        QSensorGestureRecognizer * rec = QSensorGestureManager::sensorGestureRecognizer(id);
        if (rec != 0) {
            d_ptr->m_sensorRecognizers.append(rec);
            d_ptr->availableIds.append(id);
        } else {
            d_ptr->invalidIds.append(id);
            //add to not available things
        }
    }

    d_ptr->meta = 0;

    QMetaObjectBuilder builder;
    builder.setSuperClass(&QObject::staticMetaObject);
    builder.setClassName("QSensorGesture");

    Q_FOREACH (QSensorGestureRecognizer *recognizer,  d_ptr->m_sensorRecognizers) {
        Q_FOREACH (const QString &gesture, recognizer->gestureSignals()) {
            QMetaMethodBuilder b =  builder.addSignal(gesture.toLatin1());
            if (!d_ptr->localGestureSignals.contains(QLatin1String(b.signature())))
                d_ptr->localGestureSignals.append(QLatin1String(b.signature()));
        }
        recognizer->createBackend();
    }
    d_ptr->meta = builder.toMetaObject();

    if (d_ptr->m_sensorRecognizers.count() > 0) {
        d_ptr->valid = true;
    }
}

/*!
  Destroy the QSensorGesture
  */
QSensorGesture::~QSensorGesture()
{
    stopDetection();
    if (d_ptr->meta)
        free(d_ptr->meta);
    delete d_ptr;
}

/*!
    Returns the gesture recognizer ids that were found.
  */
QStringList QSensorGesture::validIds() const
{
    return d_ptr->availableIds;
}

/*!
   Returns the gesture recognizer ids that were not found.
  */
QStringList QSensorGesture::invalidIds() const
{
    return d_ptr->invalidIds;
}

/*!
  Starts the gesture detection routines in the recognizer.
  */
void QSensorGesture::startDetection()
{
    if (d_ptr->m_sensorRecognizers.count() < 1)
        return;
    if (d_ptr->isActive)
        return;

    Q_FOREACH (QSensorGestureRecognizer *recognizer,  d_ptr->m_sensorRecognizers) {

        Q_ASSERT(recognizer !=0);

        connect(recognizer,SIGNAL(detected(QString)),
                this,SIGNAL(detected(QString)),Qt::UniqueConnection);

        //connect recognizer signals
        Q_FOREACH (QString method, recognizer->gestureSignals()) {
            method.prepend(QLatin1String("2"));
            connect(recognizer, method.toLatin1(),
                    this, method.toLatin1(), Qt::UniqueConnection);
        }

        recognizer->startBackend();
    }
    d_ptr->isActive = true;
}

/*!
  Stops the gesture detection routines.
  */
void QSensorGesture::stopDetection()
{
    if (d_ptr->m_sensorRecognizers.count() < 1)
        return;

    if (!d_ptr->isActive)
        return;

    Q_FOREACH (QSensorGestureRecognizer *recognizer,  d_ptr->m_sensorRecognizers) {
        disconnect(recognizer,SIGNAL(detected(QString)),
                   this,SIGNAL(detected(QString)));
        //disconnect recognizer signals
        Q_FOREACH (QString method,recognizer->gestureSignals()) {
            method.prepend(QLatin1String("2"));
            disconnect(recognizer, method.toLatin1(),
                       this, method.toLatin1());
        }

        recognizer->stopBackend();
    }
    d_ptr->isActive = false;
}

/*!
  Returns all the possible gestures signals that may be emitted.
  */
QStringList QSensorGesture::gestureSignals() const
{
    if (d_ptr->m_sensorRecognizers.count() > 0) {
        return  d_ptr->localGestureSignals;
    }
    return QStringList();
}

/*!
  Returns whether this gesture is active or not.
  */

bool QSensorGesture::isActive()
{
    return d_ptr->isActive;
}

/*!
  \internal
*/
const QMetaObject* QSensorGesture::metaObject() const
{
    return d_ptr->meta;
}
/*!
  \internal
*/
int QSensorGesture::qt_metacall(QMetaObject::Call c, int id, void **a)
{
    id = QObject::qt_metacall(c, id, a);

    if (id < 0 || !d_ptr->meta)
        return id;

    QMetaObject::activate(this, d_ptr->meta, id, a);
    return id;
}

QSensorGesturePrivate::QSensorGesturePrivate(QObject *parent)
    : QObject(parent),isActive(0), valid(0)
{
}

QSensorGesturePrivate::~QSensorGesturePrivate()
{

}
