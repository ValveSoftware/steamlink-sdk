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

#ifndef QWEBENGINEURLREQUESTINFO_H
#define QWEBENGINEURLREQUESTINFO_H

#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#include <QtCore/qscopedpointer.h>
#include <QtCore/qurl.h>

namespace QtWebEngineCore {
class NetworkDelegateQt;
}

QT_BEGIN_NAMESPACE

class QWebEngineUrlRequestInfoPrivate;

class QWEBENGINE_EXPORT QWebEngineUrlRequestInfo {
public:
    enum ResourceType {
        ResourceTypeMainFrame = 0,  // top level page
        ResourceTypeSubFrame,       // frame or iframe
        ResourceTypeStylesheet,     // a CSS stylesheet
        ResourceTypeScript,         // an external script
        ResourceTypeImage,          // an image (jpg/gif/png/etc)
        ResourceTypeFontResource,   // a font
        ResourceTypeSubResource,    // an "other" subresource.
        ResourceTypeObject,         // an object (or embed) tag for a plugin,
                                    // or a resource that a plugin requested.
        ResourceTypeMedia,          // a media resource.
        ResourceTypeWorker,         // the main resource of a dedicated worker.
        ResourceTypeSharedWorker,   // the main resource of a shared worker.
        ResourceTypePrefetch,       // an explicitly requested prefetch
        ResourceTypeFavicon,        // a favicon
        ResourceTypeXhr,            // a XMLHttpRequest
        ResourceTypePing,           // a ping request for <a ping>
        ResourceTypeServiceWorker,  // the main resource of a service worker.
        ResourceTypeCspReport,      // Content Security Policy (CSP) violation report
        ResourceTypePluginResource, // A resource requested by a plugin
#ifndef Q_QDOC
        ResourceTypeLast,
#endif
        ResourceTypeUnknown = 255
    };

    enum NavigationType {
        NavigationTypeLink,
        NavigationTypeTyped,
        NavigationTypeFormSubmitted,
        NavigationTypeBackForward,
        NavigationTypeReload,
        NavigationTypeOther
    };

    ResourceType resourceType() const;
    NavigationType navigationType() const;

    QUrl requestUrl() const;
    QUrl firstPartyUrl() const;
    QByteArray requestMethod() const;
    bool changed() const;

    void block(bool shouldBlock);
    void redirect(const QUrl &url);
    void setHttpHeader(const QByteArray &name, const QByteArray &value);

private:
    friend class QtWebEngineCore::NetworkDelegateQt;
    Q_DISABLE_COPY(QWebEngineUrlRequestInfo)
    Q_DECLARE_PRIVATE(QWebEngineUrlRequestInfo)

    QWebEngineUrlRequestInfo(QWebEngineUrlRequestInfoPrivate *p);
    ~QWebEngineUrlRequestInfo();
    QScopedPointer<QWebEngineUrlRequestInfoPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QWEBENGINEURLREQUESTINFO_H
