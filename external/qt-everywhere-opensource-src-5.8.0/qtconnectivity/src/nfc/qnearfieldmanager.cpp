/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnearfieldmanager.h"
#include "qnearfieldmanager_p.h"

#if defined(QT_SIMULATOR)
#include "qnearfieldmanager_simulator_p.h"
#elif defined(NEARD_NFC)
#include "qnearfieldmanager_neard_p.h"
#elif defined(ANDROID_NFC)
#include "qnearfieldmanager_android_p.h"
#else
#include "qnearfieldmanagerimpl_p.h"
#endif

#include <QtCore/QMetaType>
#include <QtCore/QMetaMethod>

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldManager
    \brief The QNearFieldManager class provides access to notifications for NFC events.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.2

    NFC Forum devices support two modes of communications.  The first mode, peer-to-peer
    communications, is used to communicate between two NFC Forum devices.  The second mode,
    master/slave communications, is used to communicate between an NFC Forum device and an NFC
    Forum Tag or Contactless Card.  The targetDetected() signal is emitted when a target device
    enters communications range.  Communications can be initiated from the slot connected to this
    signal.

    NFC Forum devices generally operate as the master in master/slave communications. Some devices
    are also capable of operating as the slave, so called Card Emulation mode. In this mode the
    local NFC device emulates a NFC Forum Tag or Contactless Card.

    NFC Forum Tags can contain one or more messages in a standardized format. These messages are
    encapsulated by the QNdefMessage class. Use the registerNdefMessageHandler() functions to
    register message handlers with particular criteria. Handlers can be unregistered with the
    unregisterNdefMessageHandler() function.

    Applications can connect to the targetDetected() and targetLost() signals to get notified when
    an NFC Forum Tag enters or leaves proximity. Before these signals are
    emitted target detection must be started with the startTargetDetection() function.
    Target detection can be stopped with
    the stopTargetDetection() function. Before a detected target can be accessed it is necessary to
    request access rights. This must be done before the target device is touched. The
    setTargetAccessModes() function is used to set the types of access the application wants to
    perform on the detected target. When access is no longer required the target access modes
    should be set to NoTargetAccess as other applications may be blocked from accessing targets.
    The current target access modes can be retried with the targetAccessModes() function.


    \section2 Automatically launching NDEF message handlers

    On some platforms it is possible to pre-register an application to receive NDEF messages
    matching a given criteria. This is useful to get the system to automatically launch your
    application when a matching NDEF message is received. This removes the need to have the user
    manually launch NDEF handling applications, prior to touching a tag, or to have those
    applications always running and using system resources.

    The process of registering the handler is different for each platform. Please refer to the
    platform documentation on how such a registration may be done.
    If the application has been registered as an NDEF message handler, the application only needs
    to call the registerNdefMessageHandler() function:

    \snippet doc_src_qtnfc.cpp handleNdefMessage

    Automatically launching NDEF message handlers is supported on
    \l{nfc-android.html}{Android}.

    \section3 NFC on Linux
    The \l{https://01.org/linux-nfc}{Linux NFC project} provides software to support NFC on Linux platforms.
    The neard daemon will allow access to the supported hardware via DBus interfaces. QtNfc requires neard
    version 0.14 which can be built from source or installed via the appropriate Linux package manager. Not
    all API features are currently supported.
    To allow QtNfc to access the DBus interfaces the neard daemon has to be running. In case of problems
    debug output can be enabled by enabling categorized logging for 'qt.nfc.neard'.
*/

/*!
    \enum QNearFieldManager::TargetAccessMode

    This enum describes the different access modes an application can have.

    \value NoTargetAccess           The application cannot access NFC capabilities.
    \value NdefReadTargetAccess     The application can read NDEF messages from targets by calling
                                    QNearFieldTarget::readNdefMessages().
    \value NdefWriteTargetAccess    The application can write NDEF messages to targets by calling
                                    QNearFieldTarget::writeNdefMessages().
    \value TagTypeSpecificTargetAccess  The application can access targets using raw commands by
                                        calling QNearFieldTarget::sendCommand().
*/

/*!
    \fn void QNearFieldManager::targetDetected(QNearFieldTarget *target)

    This signal is emitted whenever a target is detected. The \a target parameter represents the
    detected target.

    This signal will be emitted for all detected targets.

    QNearFieldManager maintains ownership of \a target, however, it will not be destroyed until
    the QNearFieldManager destructor is called. Ownership may be transferred by calling
    setParent().

    Do not delete \a target from the slot connected to this signal, instead call deleteLater().

    \note that if \a target is deleted before it moves out of proximity the targetLost() signal
    will not be emitted.

    \sa targetLost()
*/

/*!
    \fn void QNearFieldManager::targetLost(QNearFieldTarget *target)

    This signal is emitted whenever a target moves out of proximity. The \a target parameter
    represents the lost target.

    Do not delete \a target from the slot connected to this signal, instead use deleteLater().

    \sa QNearFieldTarget::disconnected()
*/

/*!
    Constructs a new near field manager with \a parent.
*/
QNearFieldManager::QNearFieldManager(QObject *parent)
:   QObject(parent), d_ptr(new QNearFieldManagerPrivateImpl)
{
    connect(d_ptr, SIGNAL(targetDetected(QNearFieldTarget*)),
            this, SIGNAL(targetDetected(QNearFieldTarget*)));
    connect(d_ptr, SIGNAL(targetLost(QNearFieldTarget*)),
            this, SIGNAL(targetLost(QNearFieldTarget*)));
}

/*!
    \internal

    Constructs a new near field manager with the specified \a backend and with \a parent.

    \note: This constructor is only enable for internal builds and is used for testing the
    simulator backend.
*/
QNearFieldManager::QNearFieldManager(QNearFieldManagerPrivate *backend, QObject *parent)
:   QObject(parent), d_ptr(backend)
{
    connect(d_ptr, SIGNAL(targetDetected(QNearFieldTarget*)),
            this, SIGNAL(targetDetected(QNearFieldTarget*)));
    connect(d_ptr, SIGNAL(targetLost(QNearFieldTarget*)),
            this, SIGNAL(targetLost(QNearFieldTarget*)));
}

/*!
    Destroys the near field manager.
*/
QNearFieldManager::~QNearFieldManager()
{
    delete d_ptr;
}

/*!
    Returns true if NFC functionality is available; otherwise returns false.
*/
bool QNearFieldManager::isAvailable() const
{
    Q_D(const QNearFieldManager);

    return d->isAvailable();
}

/*!
    \fn bool QNearFieldManager::startTargetDetection()

    Starts detecting targets and returns true if target detection is
    successfully started; otherwise returns false. Causes the targetDetected() signal to be emitted
    when a target is within proximity.
    \sa stopTargetDetection()

    \note For platforms using neard: target detection will stop as soon as a tag has been detected.
*/
bool QNearFieldManager::startTargetDetection()
{
    Q_D(QNearFieldManager);
    return d->startTargetDetection();
}

/*!
    Stops detecting targets.  The targetDetected() signal will no longer be emitted until another
    call to startTargetDetection() is made.
*/
void QNearFieldManager::stopTargetDetection()
{
    Q_D(QNearFieldManager);

    d->stopTargetDetection();
}

static QMetaMethod methodForSignature(QObject *object, const char *method)
{
    QByteArray normalizedMethod = QMetaObject::normalizedSignature(method);

    if (!QMetaObject::checkConnectArgs(SIGNAL(targetDetected(QNdefMessage,QNearFieldTarget*)),
                                       normalizedMethod)) {
        qWarning("Signatures do not match: %s:%d\n", __FILE__, __LINE__);
        return QMetaMethod();
    }

    quint8 memcode = (normalizedMethod.at(0) - '0') & 0x03;
    normalizedMethod = normalizedMethod.mid(1);

    int index;
    switch (memcode) {
    case QSLOT_CODE:
        index = object->metaObject()->indexOfSlot(normalizedMethod.constData());
        break;
    case QSIGNAL_CODE:
        index = object->metaObject()->indexOfSignal(normalizedMethod.constData());
        break;
    case QMETHOD_CODE:
        index = object->metaObject()->indexOfMethod(normalizedMethod.constData());
        break;
    default:
        index = -1;
    }

    if (index == -1)
        return QMetaMethod();

    return object->metaObject()->method(index);
}

/*!
    Registers \a object to receive notifications on \a method when a tag has been detected and has
    an NDEF record that matches \a typeNameFormat and \a type.  The \a method on \a object should
    have the prototype
    'void targetDetected(const QNdefMessage &message, QNearFieldTarget *target)'.

    Returns an identifier, which can be used to unregister the handler, on success; otherwise
    returns -1.

    \note The \e target parameter of \a method may not be available on all platforms, in which case
    \e target will be 0.

    \note On platforms using neard registering message handlers is not supported.
*/

int QNearFieldManager::registerNdefMessageHandler(QNdefRecord::TypeNameFormat typeNameFormat,
                                                  const QByteArray &type,
                                                  QObject *object, const char *method)
{
    QMetaMethod metaMethod = methodForSignature(object, method);
    if (!metaMethod.enclosingMetaObject())
        return -1;

    QNdefFilter filter;
    filter.appendRecord(typeNameFormat, type);

    Q_D(QNearFieldManager);

    return d->registerNdefMessageHandler(filter, object, metaMethod);
}

/*!
    \fn int QNearFieldManager::registerNdefMessageHandler(QObject *object, const char *method)

    Registers \a object to receive notifications on \a method when a tag has been detected and has
    an NDEF message that matches a pre-registered message format. The \a method on \a object should
    have the prototype
    'void targetDetected(const QNdefMessage &message, QNearFieldTarget *target)'.

    Returns an identifier, which can be used to unregister the handler, on success; otherwise
    returns -1.

    This function is used to register a QNearFieldManager instance to receive notifications when a
    NDEF message matching a pre-registered message format is received. See the section on
    \l {Automatically launching NDEF message handlers}.

    \note The \e target parameter of \a method may not be available on all platforms, in which case
    \e target will be 0.
*/
int QNearFieldManager::registerNdefMessageHandler(QObject *object, const char *method)
{
    QMetaMethod metaMethod = methodForSignature(object, method);
    if (!metaMethod.enclosingMetaObject())
        return -1;

    Q_D(QNearFieldManager);

    return d->registerNdefMessageHandler(object, metaMethod);
}

/*!
    Registers \a object to receive notifications on \a method when a tag has been detected and has
    an NDEF message that matches \a filter is detected. The \a method on \a object should have the
    prototype 'void targetDetected(const QNdefMessage &message, QNearFieldTarget *target)'.

    Returns an identifier, which can be used to unregister the handler, on success; otherwise
    returns -1.

    \note The \e target parameter of \a method may not be available on all platforms, in which case
    \e target will be 0.
*/
int QNearFieldManager::registerNdefMessageHandler(const QNdefFilter &filter,
                                                  QObject *object, const char *method)
{
    QMetaMethod metaMethod = methodForSignature(object, method);
    if (!metaMethod.enclosingMetaObject())
        return -1;

    Q_D(QNearFieldManager);

    return d->registerNdefMessageHandler(filter, object, metaMethod);
}

/*!
    Unregisters the target detect handler identified by \a handlerId.

    Returns true on success; otherwise returns false.
*/
bool QNearFieldManager::unregisterNdefMessageHandler(int handlerId)
{
    Q_D(QNearFieldManager);

    return d->unregisterNdefMessageHandler(handlerId);
}

/*!
    Sets the requested target access modes to \a accessModes.
*/
void QNearFieldManager::setTargetAccessModes(TargetAccessModes accessModes)
{
    Q_D(QNearFieldManager);

    TargetAccessModes removedModes = ~accessModes & d->m_requestedModes;
    if (removedModes)
        d->releaseAccess(removedModes);

    TargetAccessModes newModes = accessModes & ~d->m_requestedModes;
    if (newModes)
        d->requestAccess(newModes);
}

/*!
    Returns current requested target access modes.
*/
QNearFieldManager::TargetAccessModes QNearFieldManager::targetAccessModes() const
{
    Q_D(const QNearFieldManager);

    return d->m_requestedModes;
}

QT_END_NAMESPACE
