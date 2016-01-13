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

#include <Enginio/private/enginioclient_p.h>
#include <Enginio/private/chunkdevice_p.h>
#include <Enginio/enginioreply.h>
#include <Enginio/private/enginioreply_p.h>
#include <Enginio/enginiomodel.h>
#include <Enginio/enginioidentity.h>
#include <Enginio/enginiooauth2authentication.h>

#include <QtCore/qthreadstorage.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qnetworkreply.h>

#if defined(ENGINIO_VALGRIND_DEBUG)
#include <QtNetwork/qsslcipher.h>
#include <QtNetwork/qsslconfiguration.h>
#endif

QT_BEGIN_NAMESPACE

/*!
  \module enginio-client
  \title Enginio Client Interface

  This interface provides access to the Enginio service through a set of C++ classes
  based on Qt.
*/

/*!
  \class EnginioClient
  \since 5.3
  \inmodule enginio-qt
  \ingroup enginio-client
  \target EnginioClientCpp
  \brief EnginioClient handles all communication with the Enginio server

  The Enginio server supports several separate "backends" with each account.
  By setting the \l{EnginioClientConnection::backendId}{backendId} a backend is chosen.
  After setting the ID interaction with the server is possible.
  The information about the backend is available on the Enginio Dashboard
  after logging in to \l {http://engin.io}{Enginio}.
  \code
    EnginioClient *client = new EnginioClient(parent);
    client->setBackendId(QByteArrayLiteral("YOUR_BACKEND_ID"));
  \endcode

  The basic functions used to interact with the backend are
  \l create(), \l query(), \l remove() and \l update().
  It is possible to do a fulltext search on the server using \l fullTextSearch().
  For file handling \l downloadUrl() and \l uploadFile() are provided.
  The functions are asynchronous, which means that they are not blocking
  and the result of them will be delivered together with EnginioReply::finished()
  signal.

  \note After the request has finished, it is the responsibility of the
  user to delete the EnginioReply object at an appropriate time.
  Do not directly delete it inside the slot connected to finished().
  You can use the \l{QObject::deleteLater()}{deleteLater()} function.

  In order to make queries that return an array of data more convenient
  a model is provided by \l {EnginioModelCpp}{EnginioModel}.
*/

/*!
  \class EnginioClientConnection
  \since 5.3
  \inmodule enginio-qt
  \ingroup enginio-client
  \brief EnginioClientConnection keeps track of the authenticated connection to the server.

  You should never use EnginioClientConnection explicitly, instead use the derrived
  EnginioClient.
  \sa EnginioClient
*/

/*!
  \namespace Enginio
  \inmodule enginio-qt
  \ingroup enginio-namespace
  \brief The Enginio namespace provides enums used throughout Enginio.
*/

/*!
  \fn void EnginioClient::error(EnginioReply *reply)
  \brief This signal is emitted when a request to the backend returns an error.

  The \a reply contains the details about the error that occured.
  \sa EnginioReply
*/

/*!
  \fn void EnginioClient::finished(EnginioReply *reply)
  \brief This signal is emitted when a request to the backend finishes.

  The \a reply contains the data returned. This signal is emitted for both, successful requests and failed ones.

  From this moment on ownership of the \a reply is moved from EnginioClient, therefore it is the developer's
  responsibility to delete the \a reply after this signal is handled. That can be achieved
  by calling the \l{QObject::deleteLater()}{deleteLater()} method of the \a reply.
  \sa EnginioReply
*/

/*!
  \property EnginioClientConnection::authenticationState
  \brief The state of the authentication.

  Enginio provides convenient user management.
  The authentication state reflects whether the current user is authenticated.
  \sa Enginio::AuthenticationState identity EnginioOAuth2Authentication
*/

/*!
  \fn EnginioClient::sessionAuthenticated(EnginioReply *reply) const
  \brief Emitted when a user logs in.

  The signal is emitted after a user was successfully logged into the backend. From that
  point on, all communication with the backend will be using these credentials.
  The \a reply contains the information about the login and the user. The details may be different
  depending on the authentication method used, but a typical reply look like this:
  \code
  {
    "access_token": "...",              // oauth2 access token
    "refresh_token": "...",             // oauth2 refresh token
    "token_type": "bearer",             // oauth2 token type
    "expires_in": 28799,                // oautth2 token expiry date
    "enginio_data": {
      "user": {
        "id": "...",                    // this user Id
        "createdAt": "...",             // when the user was created
        "creator": {                    // who created the user
          "id": "creatorId",
          "objectType": "users"
        },
        "email": "user@user.com",       // the user's email address
        "firstName": "John",            // the user's first name
        "lastName": "Foo",              // the user's last name
        "objectType": "users",
        "updatedAt": "2013-11-25T14:54:58.957Z",
        "username": "JohnFoo"           // the user's login
      },
      "usergroups": []                  // usergroups to which the user belongs
    }
  }
  \endcode

  \note The \a reply will be deleted automatically after this signal, so it can not be
  stored.

  \sa sessionAuthenticationError(), EnginioReply, EnginioOAuth2Authentication
*/

/*!
  \fn EnginioClient::sessionAuthenticationError(EnginioReply *reply) const
  \brief Emitted when a user login fails.

  The \a reply contains the details about why the login failed.
  \sa sessionAuthenticated(), EnginioReply EnginioClientConnection::identity() EnginioOAuth2Authentication

  \note The \a reply will be deleted automatically after this signal, so it can not be
  stored.
*/

/*!
  \fn EnginioClient::sessionTerminated() const
  \brief Emitted when a user logs out.
  \sa EnginioClientConnection::identity() EnginioOAuth2Authentication
*/

/*!
    \enum Enginio::Operation

    Enginio has a unified API for several \c operations.
    For example when using query(), the default is \c ObjectOperation,
    which means that the query will return objects from the database.
    When passing in UserOperation instead, the query will return
    users.

    \value ObjectOperation Operate on objects
    \value AccessControlOperation Operate on the ACL
    \value FileOperation Operate with files
    \value UserOperation Operate on users
    \value UsergroupOperation Operate on groups
    \value UsergroupMembersOperation Operate on group members

    \omitvalue SessionOperation
    \omitvalue SearchOperation
    \omitvalue FileChunkUploadOperation
    \omitvalue FileGetDownloadUrlOperation
*/

/*!
    \enum Enginio::AuthenticationState

    This enum describes the state of the user authentication.
    \value NotAuthenticated No attempt to authenticate was made
    \value Authenticating Authentication request has been sent to the server
    \value Authenticated Authentication was successful
    \value AuthenticationFailure Authentication failed

    \sa EnginioClient::authenticationState
*/

ENGINIOCLIENT_EXPORT bool gEnableEnginioDebugInfo = !qEnvironmentVariableIsSet("ENGINIO_DEBUG_INFO");

QNetworkRequest EnginioClientConnectionPrivate::prepareRequest(const QUrl &url)
{
    QByteArray requestId = QUuid::createUuid().toByteArray();

    // Remove unneeded pretty-formatting.
    // before: "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
    // after:  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    requestId.chop(1);      // }
    requestId.remove(0, 1); // {
    requestId.remove(23, 1);
    requestId.remove(18, 1);
    requestId.remove(13, 1);
    requestId.remove(8, 1);

    QNetworkRequest req(_request);
    req.setUrl(url);
    req.setRawHeader(EnginioString::X_Request_Id, requestId);
    return req;
}

bool EnginioClientConnectionPrivate::appendIdToPathIfPossible(QString *path, const QString &id, QByteArray *errorMsg, EnginioClientConnectionPrivate::PathOptions flags, QByteArray errorMessageHint)
{
    Q_ASSERT(path && errorMsg);
    if (id.isEmpty()) {
        if (flags == RequireIdInPath) {
            *errorMsg = constructErrorMessage(errorMessageHint);
            return false;
        }
        return true;
    }
    path->append('/');
    path->append(id);
    return true;
}

EnginioClientConnectionPrivate::EnginioClientConnectionPrivate() :
    _identity(),
    _serviceUrl(EnginioString::apiEnginIo),
    _networkManager(),
    _uploadChunkSize(512 * 1024),
    _authenticationState(Enginio::NotAuthenticated)
{
    assignNetworkManager();

#if defined(ENGINIO_VALGRIND_DEBUG)
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setCiphers(QList<QSslCipher>() << QSslCipher(QStringLiteral("ECDHE-RSA-DES-CBC3-SHA"), QSsl::SslV3));
    _request.setSslConfiguration(conf);
#endif

    _request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json"));
}

void EnginioClientConnectionPrivate::init()
{
    QObject::connect(static_cast<EnginioClient*>(q_ptr), &EnginioClient::sessionTerminated, AuthenticationStateTrackerFunctor(this));
    QObject::connect(static_cast<EnginioClient*>(q_ptr), &EnginioClient::sessionAuthenticated, AuthenticationStateTrackerFunctor(this, Enginio::Authenticated));
    QObject::connect(static_cast<EnginioClient*>(q_ptr), &EnginioClient::sessionAuthenticationError, AuthenticationStateTrackerFunctor(this, Enginio::AuthenticationFailure));
    _request.setHeader(QNetworkRequest::UserAgentHeader,
                          QByteArrayLiteral("Qt:" QT_VERSION_STR " Enginio:" ENGINIO_VERSION " Language:C++"));
}

void EnginioClientConnectionPrivate::replyFinished(QNetworkReply *nreply)
{
    EnginioReplyState *ereply = _replyReplyMap.take(nreply);

    if (!ereply)
        return;

    if (nreply->error() != QNetworkReply::NoError) {
        QPair<QIODevice *, qint64> deviceState = _chunkedUploads.take(nreply);
        delete deviceState.first;
        emitError(ereply);
    }

    // continue chunked upload
    else if (_chunkedUploads.contains(nreply)) {
        QPair<QIODevice *, qint64> deviceState = _chunkedUploads.take(nreply);
        QString status = ereply->data().value(EnginioString::status).toString();
        if (status == EnginioString::empty || status == EnginioString::incomplete) {
            Q_ASSERT(ereply->data().value(EnginioString::objectType).toString() == EnginioString::files);
            uploadChunk(ereply, deviceState.first, deviceState.second);
            return;
        }
        // should never get here unless upload was successful
        Q_ASSERT(status == EnginioString::complete);
        delete deviceState.first;
        if (_connections.count() * 2 > _chunkedUploads.count()) {
            _connections.removeAll(QMetaObject::Connection());
        }
    }

    if (Q_UNLIKELY(ereply->delayFinishedSignal())) {
        // delay emittion of finished signal for autotests
        _delayedReplies.insert(ereply);
    } else {
        ereply->dataChanged();
        EnginioReplyStatePrivate::get(ereply)->emitFinished();
        emitFinished(ereply);
        if (gEnableEnginioDebugInfo)
            _requestData.remove(nreply);
    }

    if (Q_UNLIKELY(_delayedReplies.count())) {
        finishDelayedReplies();
    }
}

bool EnginioClientConnectionPrivate::finishDelayedReplies()
{
    // search if we can trigger an old finished signal.
    bool needToReevaluate = false;
    do {
        needToReevaluate = false;
        foreach (EnginioReplyState *reply, _delayedReplies) {
            if (!reply->delayFinishedSignal()) {
                reply->dataChanged();
                EnginioReplyStatePrivate::get(reply)->emitFinished();
                emitFinished(reply);
                if (gEnableEnginioDebugInfo)
                    _requestData.remove(reply->d_func()->_nreply); // FIXME it is ugly, and breaks encapsulation
                _delayedReplies.remove(reply);
                needToReevaluate = true;
            }
        }
    } while (needToReevaluate);
    return !_delayedReplies.isEmpty();
}

EnginioClientConnectionPrivate::~EnginioClientConnectionPrivate()
{
    foreach (const QMetaObject::Connection &identityConnection, _identityConnections)
        QObject::disconnect(identityConnection);
    foreach (const QMetaObject::Connection &connection, _connections)
        QObject::disconnect(connection);
    QObject::disconnect(_networkManagerConnection);
}

class EnginioClientPrivate: public EnginioClientConnectionPrivate {
public:
    EnginioClientPrivate()
    {}
};

/*!
  \brief Creates a new EnginioClient with \a parent as QObject parent.
*/
EnginioClient::EnginioClient(QObject *parent)
    : EnginioClientConnection(*new EnginioClientPrivate, parent)
{
    Q_D(EnginioClient);
    d->init();
}

/*!
 * Destroys the EnginioClient.
 *
 * This ends the Enginio session.
 */
EnginioClient::~EnginioClient()
{}

/*!
 * \property EnginioClientConnection::backendId
 * \brief The unique ID for the used Enginio backend.
 *
 * The backend ID determines which Enginio backend is used
 * by this instance of EnginioClient. The backend ID is
 * required for Enginio to work.
 * It is possible to use several Enginio backends simultaneously
 * by having several instances of EnginioClient.
 */
QByteArray EnginioClientConnection::backendId() const
{
    Q_D(const EnginioClientConnection);
    return d->_backendId;
}

void EnginioClientConnection::setBackendId(const QByteArray &backendId)
{
    Q_D(EnginioClientConnection);
    if (d->_backendId != backendId) {
        d->_backendId = backendId;
        d->_request.setRawHeader("Enginio-Backend-Id", d->_backendId);
        emit backendIdChanged(backendId);
    }
}

/*!
  \property EnginioClientConnection::serviceUrl
  \brief Enginio backend URL.

  The API URL determines the server used by Enginio.

  Usually it is not needed to change the default URL, but if it has
  to be changed it should be done as a first operaion on this
  EnginioClientConnection, otherwise some request may be sent accidentally
  to the default url.
*/
QUrl EnginioClientConnection::serviceUrl() const
{
    Q_D(const EnginioClientConnection);
    return d->_serviceUrl;
}

void EnginioClientConnection::setServiceUrl(const QUrl &serviceUrl)
{
    Q_D(EnginioClientConnection);
    if (d->_serviceUrl != serviceUrl) {
        d->_serviceUrl = serviceUrl;
        emit serviceUrlChanged(serviceUrl);
    }
}

/*!
  \brief Get the QNetworkAccessManager used by the Enginio library.

  \note that this QNetworkAccessManager may be shared with other EnginioClient instances
  and it is owned by them.
*/
QNetworkAccessManager *EnginioClientConnection::networkManager() const
{
    Q_D(const EnginioClientConnection);
    return d->networkManager();
}

/*!
  \brief Create custom request to the enginio REST API

  \a url The url to be used for the request. Note that the provided url completely replaces the internal serviceUrl.
  \a httpOperation Verb to the server that is valid according to the HTTP specification (eg. "GET", "POST", "PUT", etc.).
  \a data optional JSON object possibly containing custom headers and the payload data for the request.

    {
        "headers" : { "Accept" : "application/json" }
        "payload" : { "email": "me@engin.io", "password": "password" }
    }

  \return EnginioReply containing the status and the result once it is finished.
  \sa EnginioReply, create(), query(), update(), remove()
  \internal
 */
EnginioReply *EnginioClient::customRequest(const QUrl &url, const QByteArray &httpOperation, const QJsonObject &data)
{
    Q_D(EnginioClient);
    QNetworkReply *nreply = d->customRequest(url, httpOperation, data);
    EnginioReply *ereply = new EnginioReply(d, nreply);
    return ereply;
}

/*!
  \brief Fulltext search on the Enginio backend

  The \a query is JSON sent to the backend to perform a fulltext search.
  Note that the search requires the searched properties to be indexed (on the server, configureable in the backend).


  \return EnginioReply containing the status and the result once it is finished.
  \sa EnginioReply, create(), query(), update(), remove(), {cloudaddressbook}{Address book example},
  {https://engin.io/documentation/rest/parameters/fulltext_query}{JSON request structure}
*/
EnginioReply *EnginioClient::fullTextSearch(const QJsonObject &query)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->query<QJsonObject>(query, Enginio::SearchOperation);
    EnginioReply *ereply = new EnginioReply(d, nreply);
    return ereply;
}

/*!
  \include client-query.qdocinc

  To query the database for all objects of the type "objects.todo":
  \snippet enginioclient/tst_enginioclient.cpp query-todo

  \return an EnginioReply containing the status and the result once it is finished.
 */
EnginioReply* EnginioClient::query(const QJsonObject &query, const Enginio::Operation operation)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->query<QJsonObject>(query, operation);
    EnginioReply *ereply = new EnginioReply(d, nreply);

    return ereply;
}

/*!
  \include client-create.qdocinc

  \snippet enginioclient/tst_enginioclient.cpp create-todo

  To add a new member to a usergroup, the JSON needs to look like the example below.
  \code
  {
      "id": "groupId",
      "member": { "id": "userId", "objectType": "users" }
  }
  \endcode
  It can be constructed like this:
  \snippet enginioclient/tst_enginioclient.cpp create-newmember

  \return an EnginioReply containing the status and data once it is finished.
*/
EnginioReply* EnginioClient::create(const QJsonObject &object, const Enginio::Operation operation)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->create<QJsonObject>(object, operation);
    EnginioReply *ereply = new EnginioReply(d, nreply);

    return ereply;
}

/*!
  \include client-update.qdocinc

  In C++, the updating of the ACL could be done like this:
  \snippet enginioclient/tst_enginioclient.cpp update-access

  \return an EnginioReply containing the status of the query and the data once it is finished.
*/
EnginioReply* EnginioClient::update(const QJsonObject &object, const Enginio::Operation operation)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->update<QJsonObject>(object, operation);
    EnginioReply *ereply = new EnginioReply(d, nreply);

    return ereply;
}

/*!
  \include client-remove.qdocinc

  To remove a todo object:
  \snippet enginioclient/tst_enginioclient.cpp remove-todo

  \return an EnginioReply containing the status and the data once it is finished.
*/
EnginioReply* EnginioClient::remove(const QJsonObject &object, const Enginio::Operation operation)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->remove<QJsonObject>(object, operation);
    EnginioReply *ereply = new EnginioReply(d, nreply);

    return ereply;
}

/*!
  \property EnginioClientConnection::identity
  Property that represents a user. Setting the property will create an asynchronous authentication request,
  the result of it updates \l{EnginioClientConnection::authenticationState()}{authenticationState}
  EnginioClient does not take ownership of the \a identity object. The object
  has to be valid to keep the authenticated session alive. When the identity object is deleted the session
  is terminated. It is allowed to assign a null pointer to the property to terminate the session.
  \sa EnginioIdentity EnginioOAuth2Authentication
*/
EnginioIdentity *EnginioClientConnection::identity() const
{
    Q_D(const EnginioClientConnection);
    return d->identity();
}

void EnginioClientConnection::setIdentity(EnginioIdentity *identity)
{
    Q_D(EnginioClientConnection);
    if (d->_identity == identity)
        return;
    d->setIdentity(identity);
}

/*!
  \brief Stores a \a file attached to an \a object in Enginio

  Each uploaded file needs to be associated with an object in the database.
  \note The upload will only work with the propper server setup: in the dashboard create a property
  of the type that you will use. Set this property to be a reference to files.

  Each uploaded file needs to be associated with an object in the database.

  In order to upload a file, first create an object:
  \snippet files/tst_files.cpp upload-create-object

  Then do the actual upload:
  \snippet files/tst_files.cpp upload

  Note: There is no need to directly delete files.
  Instead when the object that contains the link to the file gets deleted,
  the file will automatically be deleted as well.

  \sa downloadUrl()
*/
EnginioReply* EnginioClient::uploadFile(const QJsonObject &object, const QUrl &file)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->uploadFile<QJsonObject>(object, file);
    EnginioReply *ereply = new EnginioReply(d, nreply);

    return ereply;
}

/*!
  \brief Get a temporary URL for a file stored in Enginio

  From this URL a file can be downloaded. The URL is valid for a certain amount of time as indicated
  in the reply.

  \snippet files/tst_files.cpp download
  The propertyName can be anything, but it must be the same as the one used to upload the file with.
  This way one object can have several files attached to itself (one per propertyName).

  If a file provides several variants, it is possible to request a variant by including it in the
  \a object.
  \code
    {
        "id": "abc123",
        "variant": "thumbnail"
    }
  \endcode
*/
EnginioReply* EnginioClient::downloadUrl(const QJsonObject &object)
{
    Q_D(EnginioClient);

    QNetworkReply *nreply = d->downloadUrl<QJsonObject>(object);
    EnginioReply *ereply = new EnginioReply(d, nreply);

    return ereply;
}

Q_GLOBAL_STATIC(QThreadStorage<QWeakPointer<QNetworkAccessManager> >, NetworkManager)

void EnginioClientConnectionPrivate::assignNetworkManager()
{
    Q_ASSERT(!_networkManager);

    _networkManager = prepareNetworkManagerInThread();
    _networkManagerConnection = QObject::connect(_networkManager.data(), &QNetworkAccessManager::finished, EnginioClientConnectionPrivate::ReplyFinishedFunctor(this));
}

QSharedPointer<QNetworkAccessManager> EnginioClientConnectionPrivate::prepareNetworkManagerInThread()
{
    QSharedPointer<QNetworkAccessManager> qnam;
    qnam = NetworkManager->localData().toStrongRef();
    if (!qnam) {
        qnam = QSharedPointer<QNetworkAccessManager>(new QNetworkAccessManager());
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0) && !defined(QT_NO_SSL) && !defined(ENGINIO_VALGRIND_DEBUG)
        qnam->connectToHostEncrypted(EnginioString::apiEnginIo);
#endif
        NetworkManager->setLocalData(qnam);
    }
    return qnam;
}

Enginio::AuthenticationState EnginioClientConnection::authenticationState() const
{
    Q_D(const EnginioClientConnection);
    return d->authenticationState();
}

/*!
  \internal
  Tries to emit finished signal from all replies that used to be delayed.
  \return false if all replies were finished, true otherwise.
*/
bool EnginioClientConnection::finishDelayedReplies()
{
    Q_D(EnginioClientConnection);
    return d->finishDelayedReplies();
}

/*!
  \internal
*/
EnginioClientConnection::EnginioClientConnection(EnginioClientConnectionPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    qRegisterMetaType<EnginioClient*>();
    qRegisterMetaType<EnginioModel*>();
    qRegisterMetaType<EnginioReply*>();
    qRegisterMetaType<EnginioIdentity*>();
    qRegisterMetaType<EnginioOAuth2Authentication*>();
    qRegisterMetaType<Enginio::Operation>();
    qRegisterMetaType<Enginio::AuthenticationState>();
    qRegisterMetaType<Enginio::ErrorType>();
}

/*!
  Destroys this instance.
*/
EnginioClientConnection::~EnginioClientConnection()
{
    qDeleteAll(findChildren<EnginioReplyState *>());
}

void EnginioClientConnectionPrivate::emitSessionTerminated() const
{
    emit static_cast<EnginioClient*>(q_ptr)->sessionTerminated();
}

void EnginioClientConnectionPrivate::emitSessionAuthenticated(EnginioReplyState *reply)
{
    emit static_cast<EnginioClient*>(q_ptr)->sessionAuthenticated(static_cast<EnginioReply*>(reply));
}

void EnginioClientConnectionPrivate::emitSessionAuthenticationError(EnginioReplyState *reply)
{
    emit static_cast<EnginioClient*>(q_ptr)->sessionAuthenticationError(static_cast<EnginioReply*>(reply));
}

void EnginioClientConnectionPrivate::emitFinished(EnginioReplyState *reply)
{
    emit static_cast<EnginioClient*>(q_ptr)->finished(static_cast<EnginioReply*>(reply));
}

void EnginioClientConnectionPrivate::emitError(EnginioReplyState *reply)
{
    emit static_cast<EnginioClient*>(q_ptr)->error(static_cast<EnginioReply*>(reply));
}

EnginioReplyState *EnginioClientConnectionPrivate::createReply(QNetworkReply *nreply)
{
    return new EnginioReply(this, nreply);
}

void EnginioClientConnectionPrivate::uploadChunk(EnginioReplyState *ereply, QIODevice *device, qint64 startPos)
{
    QUrl serviceUrl = _serviceUrl;
    {
        QString path;
        QByteArray errorMsg;
        if (!getPath(ereply->data(), Enginio::FileChunkUploadOperation, &path, &errorMsg).successful())
            Q_UNREACHABLE(); // sequential upload can not have an invalid path!
        serviceUrl.setPath(path);
    }

    QNetworkRequest req = prepareRequest(serviceUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, EnginioString::Application_octet_stream);

    // Content-Range: bytes {chunkStart}-{chunkEnd}/{totalFileSize}
    qint64 size = device->size();
    qint64 endPos = qMin(startPos + _uploadChunkSize, size);
    req.setRawHeader(EnginioString::Content_Range,
                     QByteArray::number(startPos) + EnginioString::Minus
                     + QByteArray::number(endPos) + EnginioString::Div
                     + QByteArray::number(size));

    // qDebug() << "Uploading chunk from " << startPos << " to " << endPos << " of " << size;

    Q_ASSERT(device->isOpen());

    ChunkDevice *chunkDevice = new ChunkDevice(device, startPos, _uploadChunkSize);
    chunkDevice->open(QIODevice::ReadOnly);

    QNetworkReply *reply = networkManager()->put(req, chunkDevice);
    chunkDevice->setParent(reply);
    _chunkedUploads.insert(reply, qMakePair(device, endPos));
    ereply->setNetworkReply(reply);
    _connections.append(QObject::connect(reply, &QNetworkReply::uploadProgress, UploadProgressFunctor(this, reply)));
}

QByteArray EnginioClientConnectionPrivate::constructErrorMessage(const QByteArray &msg)
{
    static QByteArray msgBegin = QByteArrayLiteral("{\"errors\": [{\"message\": \"");
    static QByteArray msgEnd = QByteArrayLiteral("\",\"reason\": \"BadRequest\"}]}");
    return msgBegin + msg + msgEnd;
}

QT_END_NAMESPACE
