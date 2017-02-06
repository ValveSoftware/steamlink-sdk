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

#include "qwebengineurlrequestinfo.h"
#include "qwebengineurlrequestinfo_p.h"

#include "content/public/common/resource_type.h"

#include "web_contents_adapter_client.h"

QT_BEGIN_NAMESPACE

ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeMainFrame, content::RESOURCE_TYPE_MAIN_FRAME)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeSubFrame, content::RESOURCE_TYPE_SUB_FRAME)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeStylesheet, content::RESOURCE_TYPE_STYLESHEET)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeScript, content::RESOURCE_TYPE_SCRIPT)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeImage, content::RESOURCE_TYPE_IMAGE)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeFontResource, content::RESOURCE_TYPE_FONT_RESOURCE)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeSubResource, content::RESOURCE_TYPE_SUB_RESOURCE)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeObject, content::RESOURCE_TYPE_OBJECT)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeMedia, content::RESOURCE_TYPE_MEDIA)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeWorker, content::RESOURCE_TYPE_WORKER)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeSharedWorker, content::RESOURCE_TYPE_SHARED_WORKER)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypePrefetch, content::RESOURCE_TYPE_PREFETCH)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeFavicon, content::RESOURCE_TYPE_FAVICON)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeXhr, content::RESOURCE_TYPE_XHR)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypePing, content::RESOURCE_TYPE_PING)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeServiceWorker, content::RESOURCE_TYPE_SERVICE_WORKER)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeCspReport, content::RESOURCE_TYPE_CSP_REPORT)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypePluginResource, content::RESOURCE_TYPE_PLUGIN_RESOURCE)
ASSERT_ENUMS_MATCH(QWebEngineUrlRequestInfo::ResourceTypeLast, content::RESOURCE_TYPE_LAST_TYPE)

ASSERT_ENUMS_MATCH(QtWebEngineCore::WebContentsAdapterClient::LinkNavigation, QWebEngineUrlRequestInfo::NavigationTypeLink)
ASSERT_ENUMS_MATCH(QtWebEngineCore::WebContentsAdapterClient::TypedNavigation, QWebEngineUrlRequestInfo::NavigationTypeTyped)
ASSERT_ENUMS_MATCH(QtWebEngineCore::WebContentsAdapterClient::FormSubmittedNavigation, QWebEngineUrlRequestInfo::NavigationTypeFormSubmitted)
ASSERT_ENUMS_MATCH(QtWebEngineCore::WebContentsAdapterClient::BackForwardNavigation, QWebEngineUrlRequestInfo::NavigationTypeBackForward)
ASSERT_ENUMS_MATCH(QtWebEngineCore::WebContentsAdapterClient::ReloadNavigation, QWebEngineUrlRequestInfo::NavigationTypeReload)
ASSERT_ENUMS_MATCH(QtWebEngineCore::WebContentsAdapterClient::OtherNavigation, QWebEngineUrlRequestInfo::NavigationTypeOther)

/*!
    \class QWebEngineUrlRequestInfo
    \inmodule QtWebEngineCore
    \since 5.6
    \brief The QWebEngineUrlRequestInfo class provides information about URL requests.

    The QWebEngineUrlRequestInfo is useful for setting extra header fields for requests
    or for redirecting certain requests without payload data to another URL.
    This class cannot be instantiated or copied by the user, instead it will
    be created by Qt WebEngine and sent through the virtual function
    QWebEngineUrlRequestInterceptor::interceptRequest() if an interceptor has been set.
*/

/*!
    \class QWebEngineUrlRequestInterceptor
    \inmodule QtWebEngineCore
    \since 5.6
    \brief The QWebEngineUrlRequestInterceptor class provides an abstract base class for URL interception.

    Implementing the \l{QWebEngineUrlRequestInterceptor} interface and installing the
    interceptor on the profile enables intercepting, blocking, and modifying URL requests
    before they reach the networking stack of Chromium.

    \sa QWebEngineUrlRequestInterceptor::interceptRequest, QWebEngineUrlRequestInfo
*/

/*!
    \fn QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor(QObject * p = 0)

    Creates a new QWebEngineUrlRequestInterceptor object with \a p as parent.
*/

/*!
    \fn void QWebEngineUrlRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)

    Reimplementing this virtual function and setting the interceptor on a profile makes
    it possible to intercept URL requests. This function is executed on the IO thread,
    and therefore running long tasks here will block networking.

    \a info contains the information about the URL request and will track internally
    whether its members have been altered.

    \sa QWebEngineProfile::setRequestInterceptor
*/


QWebEngineUrlRequestInfoPrivate::QWebEngineUrlRequestInfoPrivate(QWebEngineUrlRequestInfo::ResourceType resource, QWebEngineUrlRequestInfo::NavigationType navigation, const QUrl &u, const QUrl &fpu, const QByteArray &m)
    : resourceType(resource)
    , navigationType(navigation)
    , shouldBlockRequest(false)
    , url(u)
    , firstPartyUrl(fpu)
    , method(m)
    , changed(false)
{
}

/*!
    \internal
*/

QWebEngineUrlRequestInfo::~QWebEngineUrlRequestInfo()
{

}

/*!
    \internal
*/

QWebEngineUrlRequestInfo::QWebEngineUrlRequestInfo(QWebEngineUrlRequestInfoPrivate *p)
    : d_ptr(p)
{
    d_ptr->q_ptr = this;
}

/*!
    \enum QWebEngineUrlRequestInfo::ResourceType

    This enum type holds the type of the requested resource:

    \value ResourceTypeMainFrame Top level page.
    \value ResourceTypeSubFrame  Frame or iframe.
    \value ResourceTypeStylesheet  A CSS stylesheet.
    \value ResourceTypeScript  An external script.
    \value ResourceTypeImage  An image (JPG, GIF, PNG, and so on).
    \value ResourceTypeFontResource  A font.
    \value ResourceTypeSubResource  An "other" subresource.
    \value ResourceTypeObject  An object (or embed) tag for a plugin or a resource that a plugin requested.
    \value ResourceTypeMedia  A media resource.
    \value ResourceTypeWorker  The main resource of a dedicated worker.
    \value ResourceTypeSharedWorker  The main resource of a shared worker.
    \value ResourceTypePrefetch  An explicitly requested prefetch.
    \value ResourceTypeFavicon  A favicon.
    \value ResourceTypeXhr  An XMLHttpRequest.
    \value ResourceTypePing  A ping request for <a ping>.
    \value ResourceTypeServiceWorker  The main resource of a service worker.
    \value ResourceTypeCspReport  A report of Content Security Policy (CSP)
           violations. CSP reports are in JSON format and they are delivered by
           HTTP POST requests to specified servers. (Added in Qt 5.7)
   \value ResourceTypePluginResource  A resource requested by a plugin. (Added in Qt 5.7)
    \value ResourceTypeUnknown  Unknown request type.

    \note For forward compatibility all values not matched should be treated as unknown,
    not just \c ResourceTypeUnknown.
*/

/*!
    Returns the resource type of the request.

    \sa QWebEngineUrlRequestInfo::ResourceType
*/

QWebEngineUrlRequestInfo::ResourceType QWebEngineUrlRequestInfo::resourceType() const
{
    Q_D(const QWebEngineUrlRequestInfo);
    return d->resourceType;
}

/*!
    \enum QWebEngineUrlRequestInfo::NavigationType

    This enum type describes the navigation type of the request:

    \value NavigationTypeLink Navigation initiated by clicking a link.
    \value NavigationTypeTyped Navigation explicitly initiated by typing a URL.
    \value NavigationTypeFormSubmitted Navigation submits a form.
    \value NavigationTypeBackForward Navigation initiated by a history action.
    \value NavigationTypeReload Navigation initiated by refreshing the page.
    \value NavigationTypeOther None of the above.
*/

/*!
    Returns the navigation type of the request.

    \sa QWebEngineUrlRequestInfo::NavigationType
*/

QWebEngineUrlRequestInfo::NavigationType QWebEngineUrlRequestInfo::navigationType() const
{
    Q_D(const QWebEngineUrlRequestInfo);
    return d->navigationType;
}

/*!
    Returns the requested URL.
*/

QUrl QWebEngineUrlRequestInfo::requestUrl() const
{
    Q_D(const QWebEngineUrlRequestInfo);
    return d->url;
}

/*!
    Returns the first party URL of the request.
    The first party URL is the URL of the page that issued the request.
*/

QUrl QWebEngineUrlRequestInfo::firstPartyUrl() const
{
    Q_D(const QWebEngineUrlRequestInfo);
    return d->firstPartyUrl;
}


/*!
    Returns the HTTP method of the request (for example, GET or POST).
*/

QByteArray QWebEngineUrlRequestInfo::requestMethod() const
{
    Q_D(const QWebEngineUrlRequestInfo);
    return d->method;
}

/*!
    \internal
*/
bool QWebEngineUrlRequestInfo::changed() const
{
    Q_D(const QWebEngineUrlRequestInfo);
    return d->changed;
}

/*!
    Redirects this request to \a url.
    It is only possible to redirect requests that do not have payload data, such as GET requests.
*/

void QWebEngineUrlRequestInfo::redirect(const QUrl &url)
{
    Q_D(QWebEngineUrlRequestInfo);
    d->changed = true;
    d->url = url;
}

/*!
    Blocks this request if \a shouldBlock is true, so that it will not proceed.

    This function can be used to prevent navigating away from a given domain, for example.
*/

void QWebEngineUrlRequestInfo::block(bool shouldBlock)
{
    Q_D(QWebEngineUrlRequestInfo);
    d->changed = true;
    d->shouldBlockRequest = shouldBlock;
}

/*!
    Sets the request header \a name to \a value for this request.
*/

void QWebEngineUrlRequestInfo::setHttpHeader(const QByteArray &name, const QByteArray &value)
{
    Q_D(QWebEngineUrlRequestInfo);
    d->changed = true;
    d->extraHeaders.insert(name, value);
}

QT_END_NAMESPACE
