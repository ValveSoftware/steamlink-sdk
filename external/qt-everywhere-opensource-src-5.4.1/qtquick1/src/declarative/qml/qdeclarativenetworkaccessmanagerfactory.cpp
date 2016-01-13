/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "qdeclarativenetworkaccessmanagerfactory.h"

QT_BEGIN_NAMESPACE

/*!
    \class QDeclarativeNetworkAccessManagerFactory
    \since 4.7
    \brief The QDeclarativeNetworkAccessManagerFactory class creates QNetworkAccessManager instances for a QML engine.

    A QML engine uses QNetworkAccessManager for all network access.
    By implementing a factory, it is possible to provide the QML engine
    with custom QNetworkAccessManager instances with specialized caching,
    proxy and cookies support.

    To implement a factory, subclass QDeclarativeNetworkAccessManagerFactory and
    implement the virtual create() method, then assign it to the relevant QML
    engine using QDeclarativeEngine::setNetworkAccessManagerFactory().

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

    \sa {declarative/cppextensions/networkaccessmanagerfactory}{NetworkAccessManagerFactory example}
*/

/*!
    Destroys the factory. The default implementation does nothing.
 */
QDeclarativeNetworkAccessManagerFactory::~QDeclarativeNetworkAccessManagerFactory()
{
}

/*!
    \fn QNetworkAccessManager *QDeclarativeNetworkAccessManagerFactory::create(QObject *parent)

    Creates and returns a network access manager with the specified \a parent.
    This method must return a new QNetworkAccessManager instance each time
    it is called.

    Note: this method may be called by multiple threads, so ensure the
    implementation of this method is reentrant.
*/

QT_END_NAMESPACE
