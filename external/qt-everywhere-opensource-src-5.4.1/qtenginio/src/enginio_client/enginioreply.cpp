/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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

#include <QtCore/qdebug.h>
#include <QtCore/qstring.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsondocument.h>
#include <QtNetwork/qnetworkreply.h>

#include <Enginio/enginioreplystate.h>
#include <Enginio/enginioreply.h>
#include <Enginio/private/enginioreply_p.h>
#include <Enginio/enginioclient.h>
#include <Enginio/private/enginioclient_p.h>
#include <Enginio/private/enginioobjectadaptor_p.h>

QT_BEGIN_NAMESPACE

/*!
  \class EnginioReply
  \since 5.3
  \brief The EnginioReply class contains the data from a request to the Enginio database.
  \inmodule enginio-qt
  \ingroup enginio-client

  The reply, when finished, contains information received from the server:
  \list
  \li Data - object, which is a result from an earlier request,
    see the \l {EnginioReply::data}{data} function
  \li Network status - in case of a network problem, additional information can
  be accessed through: errorType, errorString, networkError
  \li Backend status - a finished request is always associated with a backend status
  code, which is just an HTTP code, and it can be queried through backendStatus
  \endlist

  The finished signal is emitted when the query is done.

  \sa EnginioClient
*/

/*!
  \class EnginioReplyState
  \since 5.3
  \internal
*/

/*!
  \enum Enginio::ErrorType
  Describes the type of error that occured when making a request to the Enginio backend.
  \value NoError The reply returned without errors
  \value NetworkError The error was a networking problem
  \value BackendError The backend did not accept the query
*/

/*!
  \fn EnginioReply::finished(EnginioReply *reply)
  This signal is emitted when the EnginioReply \a reply is finished.
  After the network operation, use the \l{EnginioReply::isError()}{isError()} function to check for
  potential problems and then use the \l data property to access the returned data.
*/

/*!
  \fn EnginioReply::progress(qint64 bytesSent, qint64 bytesTotal)
  This signal is emitted for file operations, indicating the progress of up or downloads.
  The \a bytesSent is the current progress relative to the total \a bytesTotal.
*/

class EnginioReplyPrivate: public EnginioReplyStatePrivate {
    Q_DECLARE_PUBLIC(EnginioReply)
public:
    EnginioReplyPrivate(EnginioClientConnectionPrivate *p, QNetworkReply *reply)
        : EnginioReplyStatePrivate(p, reply)
    {}
    void emitFinished() Q_DECL_OVERRIDE;
};

/*!
  \internal
*/
EnginioReply::EnginioReply(EnginioClientConnectionPrivate *p, QNetworkReply *reply)
    : EnginioReplyState(p, reply, new EnginioReplyPrivate(p, reply))
{
    QObject::connect(this, &EnginioReply::dataChanged, this, &EnginioReplyState::dataChanged);
}

/*!
  \brief Destroys the EnginioReply.

  The reply needs to be deleted after the finished signal is emitted.
*/
EnginioReply::~EnginioReply()
{}

/*!
  \property EnginioReply::networkError
  This property holds the network error for the request.
*/

QNetworkReply::NetworkError EnginioReplyState::networkError() const
{
    Q_D(const EnginioReplyState);
    return d->errorCode();
}

/*!
  \property EnginioReply::errorString
  This property holds the error for the request as human readable string.
  Check \l{EnginioReply::isError()}{isError()} first to check if the reply is an error.
*/

QString EnginioReplyState::errorString() const
{
    Q_D(const EnginioReplyState);
    return d->errorString();
}

/*!
  \property EnginioReply::requestId
  This property holds the API request ID for the request.
  The request ID is useful for end-to-end tracking of requests and to identify
  the origin of notifications.
  \internal
*/

/*!
  \internal
*/
QString EnginioReplyState::requestId() const
{
    Q_D(const EnginioReplyState);
    return d->requestId();
}

/*!
  \property EnginioReply::data
  \brief The data returned from the backend
  This property holds the JSON data returned by the server after a successful request.
*/

QJsonObject EnginioReply::data() const
{
    Q_D(const EnginioReply);
    return d->data();
}

/*!
  \internal
*/
void EnginioReplyPrivate::emitFinished()
{
    Q_Q(EnginioReply);
    emit q->finished(q);
}

/*!
  \internal
*/
void EnginioReplyState::setNetworkReply(QNetworkReply *reply)
{
    Q_D(EnginioReplyState);
    d->setNetworkReply(reply);
}

void EnginioReplyStatePrivate::setNetworkReply(QNetworkReply *reply)
{
    Q_Q(EnginioReplyState);
    _client->unregisterReply(_nreply);

    if (gEnableEnginioDebugInfo)
        _client->_requestData.remove(_nreply);

    if (!_nreply->isFinished()) {
        _nreply->setParent(_nreply->manager());
        QObject::connect(_nreply, &QNetworkReply::finished, _nreply, &QNetworkReply::deleteLater);
        _nreply->abort();
    } else {
        _nreply->deleteLater();
    }
    _nreply = reply;
    _data = QByteArray();

    _client->registerReply(reply, q);
}

/*!
  \internal
*/
void EnginioReplyState::swapNetworkReply(EnginioReplyState *other)
{
    Q_D(EnginioReplyState);
    d->swapNetworkReply(other->d_func());
}

void EnginioReplyStatePrivate::swapNetworkReply(EnginioReplyStatePrivate *other)
{
    Q_Q(EnginioReplyState);
    Q_ASSERT(other->_client == _client);
    _client->unregisterReply(_nreply);
    _client->unregisterReply(other->_nreply);

    qSwap(_nreply, other->_nreply);
    _data = other->_data = QByteArray();

    _client->registerReply(_nreply, q);
    _client->registerReply(other->_nreply, other->q_func());
}

/*!
  \internal
*/
void EnginioReplyState::dumpDebugInfo() const
{
    Q_D(const EnginioReplyState);
    d->dumpDebugInfo();
}

/*!
  \internal
  Mark this EnginioReply as not finished, the finished signal
  will be delayed until delayFinishedSignal() is returning true.

  \note The feature can be used only with one EnginioClient
*/
void EnginioReplyState::setDelayFinishedSignal(bool delay)
{
    Q_D(EnginioReplyState);
    d->_delay = delay;
    d->_client->finishDelayedReplies();
}

/*!
  \internal
  Returns true if signal should be delayed
 */
bool EnginioReplyState::delayFinishedSignal()
{
    Q_D(EnginioReplyState);
    return d->_delay;
}

/*!
  \fn bool EnginioReply::isError() const
  Returns whether this reply was unsuccessful.
  Returns true if the reply did not succeed.
*/

bool EnginioReplyState::isError() const
{
    Q_D(const EnginioReplyState);
    return d->errorCode() != QNetworkReply::NoError;
}

/*!
  \fn bool EnginioReply::isFinished() const
  Returns whether this reply was finished or not.
  Returns true if the reply was finished.
*/

bool EnginioReplyState::isFinished() const
{
    Q_D(const EnginioReplyState);
    return d->isFinished();
}

/*!
  \property EnginioReply::backendStatus
  \return the backend return status for this reply.
  \sa Enginio::ErrorType
*/

int EnginioReplyState::backendStatus() const
{
    Q_D(const EnginioReplyState);
    return d->backendStatus();
}

/*!
  \property EnginioReply::errorType
  \return the type of the error
  \sa Enginio::ErrorType
*/

Enginio::ErrorType EnginioReplyState::errorType() const
{
    Q_D(const EnginioReplyState);
    return d->errorType();
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const EnginioReply *reply)
{
    if (!reply) {
        d << "EnginioReply(null)";
        return d;
    }
    d.nospace();
    d << "EnginioReply(" << hex << (void *) reply << dec;

    if (!reply->isError()) {
        d << " success data=" << reply->data();
    } else {
        d << " errorCode=" << reply->networkError() << " ";
        d << " errorString=" << reply->errorString() << " ";
        d << " errorData=" << reply->data() << " ";
    }
    d << "backendStatus=" << reply->backendStatus();
    d << ")";
    return d.space();
}
#endif // QT_NO_DEBUG_STREAM

EnginioReplyState::EnginioReplyState(EnginioClientConnectionPrivate *parent, QNetworkReply *reply, EnginioReplyStatePrivate *priv)
    : QObject(*priv, parent->q_ptr)
{
    parent->registerReply(reply, this);
}

EnginioReplyState::~EnginioReplyState()
{
    Q_D(EnginioReplyState);
    Q_ASSERT(d->_nreply->parent() == this);
    if (Q_UNLIKELY(!d->isFinished())) {
        QObject::connect(d->_nreply, &QNetworkReply::finished, d->_nreply, &QNetworkReply::deleteLater);
        d->_client->unregisterReply(d->_nreply);
        d->_nreply->setParent(d->_nreply->manager());
        d->_nreply->abort();
    }
}

/*!
  \internal
*/
QJsonObject EnginioReplyState::data() const
{
    Q_D(const EnginioReplyState);
    return d->data();
}

QT_END_NAMESPACE

