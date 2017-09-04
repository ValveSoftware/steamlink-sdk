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

#include <QtCore/qmetaobject.h>
#include <QtCore/qdebug.h>

#include "qmediaobject_p.h"

#include <qmediaservice.h>
#include <qmetadatareadercontrol.h>
#include <qmediabindableinterface.h>
#include <qmediaavailabilitycontrol.h>

QT_BEGIN_NAMESPACE

void QMediaObjectPrivate::_q_notify()
{
    Q_Q(QMediaObject);

    const QMetaObject* m = q->metaObject();

    for (int pi : qAsConst(notifyProperties)) {
        QMetaProperty p = m->property(pi);
        p.notifySignal().invoke(
            q, QGenericArgument(QMetaType::typeName(p.userType()), p.read(q).data()));
    }
}

void QMediaObjectPrivate::_q_availabilityChanged()
{
    Q_Q(QMediaObject);

    // Really this should not always emit, but
    // we can't really tell from here (isAvailable
    // may not have changed, or the mediaobject's overridden
    // availability() may not have changed).
    q->availabilityChanged(q->availability());
    q->availabilityChanged(q->isAvailable());
}

/*!
    \class QMediaObject

    \brief The QMediaObject class provides a common base for multimedia objects.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_core

    It provides some basic functionality that is common to other high level classes
    like \l QMediaPlayer, \l QAudioDecoder and \l QCamera, including availability
    and meta-data functionality, as well as functionality to connect media objects
    with support classes like QMediaPlaylist.

    The higher level QMediaObject derived classes provide the actual multimedia
    functionality, by internally using a QMediaService.  Each media object
    hosts a QMediaService and uses the QMediaControl interfaces implemented by the service to implement its
    API.  These controls can be accessed from the media object if necessary, but in general
    the useful functionality can be accessed from the higher level classes.

    Most media objects when constructed will request a new
    QMediaService instance, but some like
    QMediaRecorder and QAudioRecorder will share a service with another object.

    \sa QMediaService, QMediaControl
*/

/*!
    Destroys this media object.
*/

QMediaObject::~QMediaObject()
{
    delete d_ptr;
}

/*!
    Returns the availability of the functionality offered by this object.

    In some cases the functionality may not be available (for example, if
    the current operating system or platform does not provide the required
    functionality), or it may be temporarily unavailable (for example,
    audio playback during a phone call or similar).
*/

QMultimedia::AvailabilityStatus QMediaObject::availability() const
{
    if (d_func()->service == 0)
        return QMultimedia::ServiceMissing;

    if (d_func()->availabilityControl)
        return d_func()->availabilityControl->availability();

    return QMultimedia::Available;
}

/*!
    Returns true if the service is available for use.
*/

bool QMediaObject::isAvailable() const
{
    return availability() == QMultimedia::Available;
}

/*!
    Returns the media service that provides the functionality of this multimedia object.
*/

QMediaService* QMediaObject::service() const
{
    return d_func()->service;
}

int QMediaObject::notifyInterval() const
{
    return d_func()->notifyTimer->interval();
}

void QMediaObject::setNotifyInterval(int milliSeconds)
{
    Q_D(QMediaObject);

    if (d->notifyTimer->interval() != milliSeconds) {
        d->notifyTimer->setInterval(milliSeconds);

        emit notifyIntervalChanged(milliSeconds);
    }
}

/*!
    Bind \a object to this QMediaObject instance.

    This method establishes a relationship between this media object and a
    helper object. The nature of the relationship depends on both parties. This
    methods returns true if the helper was successfully bound, false otherwise.

    Most subclasses of QMediaObject provide more convenient functions
    that wrap this functionality, so this function rarely needs to be
    called directly.

    The object passed must implement the QMediaBindableInterface interface.

    \sa QMediaBindableInterface
*/
bool QMediaObject::bind(QObject *object)
{
    QMediaBindableInterface *helper = qobject_cast<QMediaBindableInterface*>(object);
    if (!helper)
        return false;

    QMediaObject *currentObject = helper->mediaObject();

    if (currentObject == this)
        return true;

    if (currentObject)
        currentObject->unbind(object);

    return helper->setMediaObject(this);
}

/*!
    Detach \a object from the QMediaObject instance.

    Unbind the helper object from this media object.  A warning
    will be generated if the object was not previously bound to this
    object.

    \sa QMediaBindableInterface
*/
void QMediaObject::unbind(QObject *object)
{
    QMediaBindableInterface *helper = qobject_cast<QMediaBindableInterface*>(object);

    if (helper && helper->mediaObject() == this)
        helper->setMediaObject(0);
    else
        qWarning() << "QMediaObject: Trying to unbind not connected helper object";
}

/*!
    Constructs a media object which uses the functionality provided by a media \a service.

    The \a parent is passed to QObject.

    This class is meant as a base class for multimedia objects so this
    constructor is protected.
*/

QMediaObject::QMediaObject(QObject *parent, QMediaService *service):
    QObject(parent),
    d_ptr(new QMediaObjectPrivate)

{
    Q_D(QMediaObject);

    d->q_ptr = this;

    d->notifyTimer = new QTimer(this);
    d->notifyTimer->setInterval(1000);
    connect(d->notifyTimer, SIGNAL(timeout()), SLOT(_q_notify()));

    d->service = service;

    setupControls();
}

/*!
    \internal
*/

QMediaObject::QMediaObject(QMediaObjectPrivate &dd, QObject *parent,
                                            QMediaService *service):
    QObject(parent),
    d_ptr(&dd)
{
    Q_D(QMediaObject);
    d->q_ptr = this;

    d->notifyTimer = new QTimer(this);
    d->notifyTimer->setInterval(1000);
    connect(d->notifyTimer, SIGNAL(timeout()), SLOT(_q_notify()));

    d->service = service;

    setupControls();
}

/*!
    Watch the property \a name. The property's notify signal will be emitted
    once every \c notifyInterval milliseconds.

    \sa notifyInterval
*/

void QMediaObject::addPropertyWatch(QByteArray const &name)
{
    Q_D(QMediaObject);

    const QMetaObject* m = metaObject();

    int index = m->indexOfProperty(name.constData());

    if (index != -1 && m->property(index).hasNotifySignal()) {
        d->notifyProperties.insert(index);

        if (!d->notifyTimer->isActive())
            d->notifyTimer->start();
    }
}

/*!
    Remove property \a name from the list of properties whose changes are
    regularly signaled.

    \sa notifyInterval
*/

void QMediaObject::removePropertyWatch(QByteArray const &name)
{
    Q_D(QMediaObject);

    int index = metaObject()->indexOfProperty(name.constData());

    if (index != -1) {
        d->notifyProperties.remove(index);

        if (d->notifyProperties.isEmpty())
            d->notifyTimer->stop();
    }
}

/*!
    \property QMediaObject::notifyInterval

    The interval at which notifiable properties will update.

    The interval is expressed in milliseconds, the default value is 1000.

    \sa addPropertyWatch(), removePropertyWatch()
*/

/*!
    \fn void QMediaObject::notifyIntervalChanged(int milliseconds)

    Signal a change in the notify interval period to \a milliseconds.
*/

/*!
    Returns true if there is meta-data associated with this media object, else false.
*/

bool QMediaObject::isMetaDataAvailable() const
{
    Q_D(const QMediaObject);

    return d->metaDataControl
            ? d->metaDataControl->isMetaDataAvailable()
            : false;
}

/*!
    \fn QMediaObject::metaDataAvailableChanged(bool available)

    Signals that the \a available state of a media object's meta-data has changed.
*/

/*!
    Returns the value associated with a meta-data \a key.

    See the list of predefined \l {QMediaMetaData}{meta-data keys}.
*/
QVariant QMediaObject::metaData(const QString &key) const
{
    Q_D(const QMediaObject);

    return d->metaDataControl
            ? d->metaDataControl->metaData(key)
            : QVariant();
}

/*!
    Returns a list of keys there is meta-data available for.
*/
QStringList QMediaObject::availableMetaData() const
{
    Q_D(const QMediaObject);

    return d->metaDataControl
            ? d->metaDataControl->availableMetaData()
            : QStringList();
}

/*!
    \fn QMediaObject::metaDataChanged()

    Signals that this media object's meta-data has changed.

    If multiple meta-data elements are changed,
    metaDataChanged(const QString &key, const QVariant &value) signal is emitted
    for each of them with metaDataChanged() changed emitted once.
*/

/*!
    \fn QMediaObject::metaDataChanged(const QString &key, const QVariant &value)

    Signal the changes of one meta-data element \a value with the given \a key.
*/


void QMediaObject::setupControls()
{
    Q_D(QMediaObject);

    if (d->service != 0) {
        d->metaDataControl = qobject_cast<QMetaDataReaderControl*>(
                d->service->requestControl(QMetaDataReaderControl_iid));

        if (d->metaDataControl) {
            connect(d->metaDataControl, SIGNAL(metaDataChanged()), SIGNAL(metaDataChanged()));
            connect(d->metaDataControl,
                    SIGNAL(metaDataChanged(QString,QVariant)),
                    SIGNAL(metaDataChanged(QString,QVariant)));
            connect(d->metaDataControl,
                    SIGNAL(metaDataAvailableChanged(bool)),
                    SIGNAL(metaDataAvailableChanged(bool)));
        }

        d->availabilityControl = d->service->requestControl<QMediaAvailabilityControl*>();
        if (d->availabilityControl) {
            connect(d->availabilityControl,
                    SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)),
                    SLOT(_q_availabilityChanged()));
        }
    }
}

/*!
    \fn QMediaObject::availabilityChanged(bool available)

    Signal emitted when the availability state has changed to \a available.
*/

/*!
    \fn QMediaObject::availabilityChanged(QMultimedia::AvailabilityStatus availability)

    Signal emitted when the availability of the service has changed to \a availability.
*/


#include "moc_qmediaobject.cpp"
QT_END_NAMESPACE

