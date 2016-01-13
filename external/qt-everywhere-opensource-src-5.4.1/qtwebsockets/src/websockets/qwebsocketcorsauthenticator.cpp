/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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

/*!
    \class QWebSocketCorsAuthenticator

    \inmodule QtWebSockets
    \since 5.3
    \brief The QWebSocketCorsAuthenticator class provides an authenticator object for
    Cross Origin Requests (CORS).

    The QWebSocketCorsAuthenticator class is used in the
    \l{QWebSocketServer::}{originAuthenticationRequired()} signal.
    The class provides a way to pass back the required information to the QWebSocketServer.
    It provides applications with fine-grained control over which origin URLs are allowed
    and which aren't.
    By default, every origin is accepted.
    To get fine-grained control, an application connects the
    \l{QWebSocketServer::}{originAuthenticationRequired()} signal to a slot.
    When the origin (QWebSocketCorsAuthenticator::origin()) is accepted,
    it calls QWebSocketCorsAuthenticator::setAllowed(true)

    \note Checking on the origin does not make much sense when the server is accessed
    via a non-browser client, as that client can set whatever origin header it likes.
    In case of a browser client, the server SHOULD check the validity of the origin.
    \sa http://tools.ietf.org/html/rfc6455#section-10

    \sa QWebSocketServer
*/

#include "qwebsocketcorsauthenticator.h"
#include "qwebsocketcorsauthenticator_p.h"

QT_BEGIN_NAMESPACE

/*!
  \internal
 */
QWebSocketCorsAuthenticatorPrivate::QWebSocketCorsAuthenticatorPrivate(const QString &origin,
                                                                       bool allowed) :
    m_origin(origin),
    m_isAllowed(allowed)
{}

/*!
  Destroys the object.
 */
QWebSocketCorsAuthenticatorPrivate::~QWebSocketCorsAuthenticatorPrivate()
{}

/*!
  Constructs a new QCorsAuthencator object with the given \a origin.
  \note By default, allowed() returns true. This means that per default every origin is accepted.
 */
QWebSocketCorsAuthenticator::QWebSocketCorsAuthenticator(const QString &origin) :
    d_ptr(new QWebSocketCorsAuthenticatorPrivate(origin, true))
{
}

/*!
  Destroys the object.
 */
QWebSocketCorsAuthenticator::~QWebSocketCorsAuthenticator()
{
}

/*!
  Constructs a copy of \a other.
 */
QWebSocketCorsAuthenticator::QWebSocketCorsAuthenticator(const QWebSocketCorsAuthenticator &other) :
    d_ptr(new QWebSocketCorsAuthenticatorPrivate(other.d_ptr->m_origin, other.d_ptr->m_isAllowed))
{
}

/*!
  Assigns \a other to this authenticator object.
 */
QWebSocketCorsAuthenticator &
QWebSocketCorsAuthenticator::operator =(const QWebSocketCorsAuthenticator &other)
{
    Q_D(QWebSocketCorsAuthenticator);
    if (this != &other) {
        d->m_origin = other.d_ptr->m_origin;
        d->m_isAllowed = other.d_ptr->m_isAllowed;
    }
    return *this;
}

#ifdef Q_COMPILER_RVALUE_REFS
/*!
  Move-constructs a QWebSocketCorsAuthenticator, making it point at the same
  object \a other was pointing to.
 */
QWebSocketCorsAuthenticator::QWebSocketCorsAuthenticator(QWebSocketCorsAuthenticator &&other) :
    d_ptr(other.d_ptr.take())
{}

/*!
  Move-assigns \a other to this instance.
 */
QWebSocketCorsAuthenticator &
QWebSocketCorsAuthenticator::operator =(QWebSocketCorsAuthenticator &&other)
{
    qSwap(d_ptr, other.d_ptr);
    return *this;
}

#endif

/*!
  Swaps \a other with this authenticator.

  This operation is very fast and never fails.
 */
void QWebSocketCorsAuthenticator::swap(QWebSocketCorsAuthenticator &other)
{
    if (&other != this)
        qSwap(d_ptr, other.d_ptr);
}

/*!
  Returns the origin this autenticator is handling about.
 */
QString QWebSocketCorsAuthenticator::origin() const
{
    Q_D(const QWebSocketCorsAuthenticator);
    return d->m_origin;
}

/*!
  Allows or disallows the origin. Setting \a allowed to true, will accept the connection request
  for the given origin.

  Setting \a allowed to false, will reject the connection request.

  \note By default, all origins are accepted.
 */
void QWebSocketCorsAuthenticator::setAllowed(bool allowed)
{
    Q_D(QWebSocketCorsAuthenticator);
    d->m_isAllowed = allowed;
}

/*!
  Returns true if the origin is allowed, otherwise returns false.

  \note By default, all origins are accepted.
 */
bool QWebSocketCorsAuthenticator::allowed() const
{
    Q_D(const QWebSocketCorsAuthenticator);
    return d->m_isAllowed;
}

QT_END_NAMESPACE
