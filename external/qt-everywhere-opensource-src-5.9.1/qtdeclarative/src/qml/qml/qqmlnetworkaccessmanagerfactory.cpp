/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlnetworkaccessmanagerfactory.h"

QT_BEGIN_NAMESPACE

#if QT_CONFIG(qml_network)

/*!
    \class QQmlNetworkAccessManagerFactory
    \since 5.0
    \inmodule QtQml
    \brief The QQmlNetworkAccessManagerFactory class creates QNetworkAccessManager instances for a QML engine.

    A QML engine uses QNetworkAccessManager for all network access.
    By implementing a factory, it is possible to provide the QML engine
    with custom QNetworkAccessManager instances with specialized caching,
    proxy and cookies support.

    To implement a factory, subclass QQmlNetworkAccessManagerFactory and
    implement the virtual create() method, then assign it to the relevant QML
    engine using QQmlEngine::setNetworkAccessManagerFactory().

    Note the QML engine may create QNetworkAccessManager instances
    from multiple threads. Because of this, the implementation of the create()
    method must be \l{Reentrancy and Thread-Safety}{reentrant}. In addition,
    the developer should be careful if the signals of the object to be
    returned from create() are connected to the slots of an object that may
    be created in a different thread:

    \list
    \li The QML engine internally handles all requests, and cleans up any
       QNetworkReply objects it creates. Receiving the
       QNetworkAccessManager::finished() signal in another thread may not
       provide the receiver with a valid reply object if it has already
       been deleted.
    \li Authentication details provided to QNetworkAccessManager::authenticationRequired()
       must be provided immediately, so this signal cannot be connected as a
       Qt::QueuedConnection (or as the default Qt::AutoConnection from another
       thread).
    \endlist

    For more information about signals and threads, see
    \l {Threads and QObjects} and \l {Signals and Slots Across Threads}.

    The \l {Qt Quick 1} version of this class is named QDeclarativeNetworkAccessManagerFactory.

    \sa {C++ Extensions: Network Access Manager Factory Example}{Network Access Manager Factory Example}
*/

/*!
    Destroys the factory. The default implementation does nothing.
 */
QQmlNetworkAccessManagerFactory::~QQmlNetworkAccessManagerFactory()
{
}

/*!
    \fn QNetworkAccessManager *QQmlNetworkAccessManagerFactory::create(QObject *parent)

    Creates and returns a network access manager with the specified \a parent.
    This method must return a new QNetworkAccessManager instance each time
    it is called.

    Note: this method may be called by multiple threads, so ensure the
    implementation of this method is reentrant.
*/

#endif // qml_network

QT_END_NAMESPACE
