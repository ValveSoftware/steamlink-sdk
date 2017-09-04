/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <qquickwebengineloadrequest_p.h>

QT_BEGIN_NAMESPACE

class QQuickWebEngineLoadRequestPrivate {
public:
    QQuickWebEngineLoadRequestPrivate(const QUrl& url, QQuickWebEngineView::LoadStatus status, const QString& errorString, int errorCode, QQuickWebEngineView::ErrorDomain errorDomain)
        : url(url)
        , status(status)
        , errorString(errorString)
        , errorCode(errorCode)
        , errorDomain(errorDomain)
    {
    }

    QUrl url;
    QQuickWebEngineView::LoadStatus status;
    QString errorString;
    int errorCode;
    QQuickWebEngineView::ErrorDomain errorDomain;
};

/*!
    \qmltype WebEngineLoadRequest
    \instantiates QQuickWebEngineLoadRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.0

    \brief A utility type for the WebEngineView::loadingChanged signal.

    Contains information about a request for loading a web page, such as the URL and
    current loading status (started, succeeded, failed).

    \sa WebEngineView::loadingChanged
*/
QQuickWebEngineLoadRequest::QQuickWebEngineLoadRequest(const QUrl& url, QQuickWebEngineView::LoadStatus status, const QString& errorString, int errorCode, QQuickWebEngineView::ErrorDomain errorDomain, QObject* parent)
    : QObject(parent)
    , d_ptr(new QQuickWebEngineLoadRequestPrivate(url, status, errorString, errorCode, errorDomain))
{
}

QQuickWebEngineLoadRequest::~QQuickWebEngineLoadRequest()
{
}

/*!
    \qmlproperty url WebEngineLoadRequest::url
    \brief Holds the URL of the load request.
 */
QUrl QQuickWebEngineLoadRequest::url() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->url;
}

/*!
    \qmlproperty enumeration WebEngineLoadRequest::status

    This enumeration represents the load status of a web page load request:

    \value  WebEngineLoadRequest.LoadStartedStatus Page is currently loading.
    \value  WebEngineLoadRequest.LoadSucceededStatus
            Page has been loaded with success.
    \value  WebEngineLoadRequest.LoadFailedStatus Page could not be loaded.

    \sa WebEngineView::loadingChanged
*/
QQuickWebEngineView::LoadStatus QQuickWebEngineLoadRequest::status() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->status;
}

/*!
    \qmlproperty string WebEngineLoadRequest::errorString
    \brief Holds the error message.
*/
QString QQuickWebEngineLoadRequest::errorString() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->errorString;
}

/*!
    \qmlproperty enumeration WebEngineLoadRequest::errorDomain
    This enumeration holds the type of a load request error:

    \value  WebEngineView.NoErrorDomain
            Error type is not known.
    \value  WebEngineView.InternalErrorDomain
            Content cannot be interpreted by Qt WebEngine.
    \value  WebEngineView.ConnectionErrorDomain
            Error results from a faulty network connection.
    \value  WebEngineView.CertificateErrorDomain
            Error is related to the SSL/TLS certificate.
    \value  WebEngineView.HttpErrorDomain
            Error is related to the HTTP connection.
    \value  WebEngineView.FtpErrorDomain
            Error is related to the FTP connection.
    \value  WebEngineView.DnsErrorDomain
            Error is related to the DNS connection.
*/

QQuickWebEngineView::ErrorDomain QQuickWebEngineLoadRequest::errorDomain() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->errorDomain;
}

/*!
    \qmlproperty int WebEngineLoadRequest::errorCode
    \brief Holds the error code.
*/
int QQuickWebEngineLoadRequest::errorCode() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->errorCode;
}

QT_END_NAMESPACE
