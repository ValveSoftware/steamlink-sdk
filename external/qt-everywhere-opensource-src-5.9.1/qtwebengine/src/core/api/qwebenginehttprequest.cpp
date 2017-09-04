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

#include "qplatformdefs.h"
#include <QtCore/qshareddata.h>
#include <QtWebEngineCore/qwebenginehttprequest.h>
#include <algorithm>

QT_BEGIN_NAMESPACE

/*!
    \class QWebEngineHttpRequest
    \since 5.9
    \ingroup webengine
    \inmodule QtWebEngineCore

    \brief The QWebEngineHttpRequest class holds a request to be sent with WebEngine.

    QWebEngineHttpRequest represents an HTTP request in the WebEngine networking stack.
    It holds the information necessary to send a request over the network. It contains
    a URL and some ancillary information that can be used to modify the request.
    Both QWebEnginePage::load() and QWebEngineView::load() accept a QWebEngineHttpRequest
    as a parameter.
*/

/*!
    \enum QWebEngineHttpRequest::Method
    \brief This enum type describes the method used to send the HTTP request:

    \value Get The GET method.
    \value Post The POST method.
*/

class QWebEngineHttpRequestPrivate : public QSharedData
{
public:
    QUrl url;
    QWebEngineHttpRequest::Method method;
    typedef QPair<QByteArray, QByteArray> HeaderPair;
    typedef QVector<HeaderPair> Headers;
    Headers headers;
    QByteArray postData;

    inline QWebEngineHttpRequestPrivate()
    {
    }

    ~QWebEngineHttpRequestPrivate()
    {
    }

    QWebEngineHttpRequestPrivate(const QWebEngineHttpRequestPrivate &other)
        : QSharedData(other)
    {
        method = other.method;
        url = other.url;
        headers = other.headers;
    }

    inline bool operator==(const QWebEngineHttpRequestPrivate &other) const
    {
        return method == other.method
            && url == other.url
            && headers == other.headers;
    }

    Headers::ConstIterator findHeader(const QByteArray &key) const;
    Headers allHeaders() const;
    QVector<QByteArray> headersKeys() const;
    void setHeader(const QByteArray &key, const QByteArray &value);
    void unsetHeader(const QByteArray &key);
    void setAllHeaders(const Headers &list);

private:
    void setHeaderInternal(const QByteArray &key, const QByteArray &value);
};

/*!
    Constructs a QWebEngineHttpRequest object with \a url as the URL to be
    requested and \a method as the method to be used.

    \sa url(), setUrl()
*/
QWebEngineHttpRequest::QWebEngineHttpRequest(const QUrl &url,
                                             const QWebEngineHttpRequest::Method &method)
    : d(new QWebEngineHttpRequestPrivate)
{
    d->method = method;
    d->url = url;
}

/*!
    Creates a copy of \a other.
*/
QWebEngineHttpRequest::QWebEngineHttpRequest(const QWebEngineHttpRequest &other)
    : d(other.d)
{
}

/*!
    Disposes of the QWebEngineHttpRequest object.
*/
QWebEngineHttpRequest::~QWebEngineHttpRequest()
{
    // QSharedDataPointer auto deletes
    d = 0;
}

/*!
    Returns \c true if this object is the same as \a other (that is, if they
    have the same method, URL, and headers).

    \sa operator!=()
*/
bool QWebEngineHttpRequest::operator==(const QWebEngineHttpRequest &other) const
{
    return d == other.d || *d == *other.d;
}

/*!
    \fn bool QWebEngineHttpRequest::operator!=(const QWebEngineHttpRequest &other) const

    Returns \c false if this object is not the same as \a other.

    \sa operator==()
*/

/*!
    Creates a copy of \a other.
*/
QWebEngineHttpRequest &QWebEngineHttpRequest::operator=(const QWebEngineHttpRequest &other)
{
    d = other.d;
    return *this;
}

/*!
    \fn void QWebEngineHttpRequest::swap(QWebEngineHttpRequest &other)

    Swaps this WebEngine request with \a other. This function is very
    fast and never fails.
*/

/*!
    Constructs a QWebEngineHttpRequest to \a url that uses the POST method.

    \note \a postData may contain arbitrary strings. They are translated
          to appropriate raw data.

    \sa postData, setPostData()
*/
QWebEngineHttpRequest QWebEngineHttpRequest::postRequest(const QUrl &url,
                                                         const QMap<QString, QString> &postData)
{
    QWebEngineHttpRequest result(url);
    result.setMethod(QWebEngineHttpRequest::Post);

    QString buffer;
    for (QMap<QString, QString>::const_iterator it = postData.begin(); it != postData.end(); it++) {
        QByteArray key = QUrl::toPercentEncoding(it.key());
        QByteArray value = QUrl::toPercentEncoding(it.value());

        if (buffer.length() > 0)
            buffer += QLatin1Char('&');
        buffer += key + QLatin1Char('=') + value;
    }
    result.setPostData(buffer.toLatin1());

    result.setHeader(QByteArrayLiteral("Content-Type"),
                     QByteArrayLiteral("application/x-www-form-urlencoded"));
    return result;
}


/*!
    Returns the method this WebEngine request is using.

    \sa setMethod()
*/
QWebEngineHttpRequest::Method QWebEngineHttpRequest::method() const
{
    return d->method;
}

/*!
    Sets the method this WebEngine request is using to be \a method.

    \sa method()
*/
void QWebEngineHttpRequest::setMethod(QWebEngineHttpRequest::Method method)
{
    d->method = method;
}

/*!
    Returns the URL this WebEngine request is referring to.

    \sa setUrl()
*/
QUrl QWebEngineHttpRequest::url() const
{
    return d->url;
}

/*!
    Sets the URL this WebEngine request is referring to be \a url.

    \sa url()
*/
void QWebEngineHttpRequest::setUrl(const QUrl &url)
{
    d->url = url;
}

/*!
    Returns the (raw) POST data this WebEngine request contains.

    \sa setPostData()
*/
QByteArray QWebEngineHttpRequest::postData() const
{
    return d->postData;
}

/*!
    Sets the (raw) POST data this WebEngine request contains to be \a postData.

    \sa postData()
*/
void QWebEngineHttpRequest::setPostData(const QByteArray &postData)
{
    d->postData = postData;
}

/*!
    Returns \c true if the header \a headerName is present in this
    WebEngine request.

    \sa setHeader(), header(), unsetHeader(), headers()
*/
bool QWebEngineHttpRequest::hasHeader(const QByteArray &headerName) const
{
    return d->findHeader(headerName) != d->headers.constEnd();
}

/*!
    Returns the header specified by \a headerName. If no such header is
    present, an empty QByteArray is returned, which may be
    indistinguishable from a header that is present but has no content
    (use hasHeader() to find out if the header exists or not).

    Headers can be set with setHeader().

    \sa setHeader(), hasHeader(), unsetHeader(), headers()
*/
QByteArray QWebEngineHttpRequest::header(const QByteArray &headerName) const
{
    QWebEngineHttpRequestPrivate::Headers::ConstIterator it =
        d->findHeader(headerName);
    if (it != d->headers.constEnd())
        return it->second;
    return QByteArray();
}

/*!
    Returns a list of all headers that are set in this WebEngine
    request. The list is in the order that the headers were set.

    \sa setHeader(), header(), hasHeader(), unsetHeader()
*/
QVector<QByteArray> QWebEngineHttpRequest::headers() const
{
    return d->headersKeys();
}

/*!
    Sets the header \a headerName to be of value \a headerValue.

    \note Setting the same header twice overrides the previous
    setting. To accomplish the behavior of multiple HTTP headers of
    the same name, you should concatenate the two values, separating
    them with a comma (",") and set one single header.

    \sa header(), hasHeader(), unsetHeader(), headers()
*/
void QWebEngineHttpRequest::setHeader(const QByteArray &headerName, const QByteArray &headerValue)
{
    d->setHeader(headerName, headerValue);
}

/*!
    Removes the header specified by \a key, if present.

    \sa setHeader(), header(), hasHeader(), headers()
*/
void QWebEngineHttpRequest::unsetHeader(const QByteArray &key)
{
    d->setHeader(key, QByteArray());
}

QWebEngineHttpRequestPrivate::Headers::ConstIterator
QWebEngineHttpRequestPrivate::findHeader(const QByteArray &key) const
{
    Headers::ConstIterator it = headers.constBegin();
    Headers::ConstIterator end = headers.constEnd();
    for ( ; it != end; ++it)
        if (qstricmp(it->first.constData(), key.constData()) == 0)
            return it;

    return end;                 // not found
}

QWebEngineHttpRequestPrivate::Headers QWebEngineHttpRequestPrivate::allHeaders() const
{
    return headers;
}

QVector<QByteArray> QWebEngineHttpRequestPrivate::headersKeys() const
{
    QVector<QByteArray> result;
    result.reserve(headers.size());
    Headers::ConstIterator it = headers.constBegin(),
                               end = headers.constEnd();
    for ( ; it != end; ++it)
        result << it->first;

    return result;
}

/*!
    \internal
    Sets the header specified by \a key to \a value.
*/
void QWebEngineHttpRequestPrivate::setHeader(const QByteArray &key, const QByteArray &value)
{
    if (key.isEmpty())
        // refuse to accept an empty header
        return;

    setHeaderInternal(key, value);
}

/*!
    \internal
    Removes the header specified by \a key, if present.
*/
void QWebEngineHttpRequestPrivate::unsetHeader(const QByteArray &key)
{
    auto firstEqualsKey = [&key](const HeaderPair &header) {
        return qstricmp(header.first.constData(), key.constData()) == 0;
    };
    headers.erase(std::remove_if(headers.begin(), headers.end(), firstEqualsKey),
                  headers.end());
}

/*!
    \internal
    Sets the internal headers list to match \a list.
*/
void QWebEngineHttpRequestPrivate::setAllHeaders(const Headers &list)
{
    headers = list;
}

/*!
    \internal
    Sets the header specified by \a key to \a value.
    \note key must not be empty. When unsure, use \a setHeader() instead.
*/
void QWebEngineHttpRequestPrivate::setHeaderInternal(const QByteArray &key, const QByteArray &value)
{
    unsetHeader(key);

    if (value.isNull())
        return;                 // only wanted to erase key

    HeaderPair pair;
    pair.first = key;
    pair.second = value;
    headers.append(pair);
}

QT_END_NAMESPACE
