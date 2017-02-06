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

#ifndef CERTIFICATE_ERROR_CONTROLLER_H
#define CERTIFICATE_ERROR_CONTROLLER_H

#include "qtwebenginecoreglobal.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

class CertificateErrorControllerPrivate;

class QWEBENGINE_EXPORT CertificateErrorController {
public:
    CertificateErrorController(CertificateErrorControllerPrivate *p);
    ~CertificateErrorController();

    // We can't use QSslError::SslErrors, because the error categories doesn't map.
    // Keep up to date with net/base/net_errors.h and net::IsCertificateError():
    enum CertificateError {
        SslPinnedKeyNotInCertificateChain = -150,
        CertificateCommonNameInvalid = -200,
        CertificateDateInvalid = -201,
        CertificateAuthorityInvalid = -202,
        CertificateContainsErrors = -203,
        CertificateNoRevocationMechanism = -204,
        CertificateUnableToCheckRevocation = -205,
        CertificateRevoked = -206,
        CertificateInvalid = -207,
        CertificateWeakSignatureAlgorithm = -208,
        CertificateNonUniqueName = -210,
        CertificateWeakKey = -211,
        CertificateNameConstraintViolation = -212,
        CertificateValidityTooLong = -213,
        CertificateTransparencyRequired = -214,

        CertificateErrorEnd = -215 // not an error, just an enum boundary
    };

    CertificateError error() const;
    QUrl url() const;
    bool overridable() const;
    bool strictEnforcement() const;
    QString errorString() const;
    QDateTime validStart() const;
    QDateTime validExpiry() const;

    void accept(bool);

    // Note: The resource type should probably not be exported, since once accepted the certificate exception
    // counts for all resource types.
    // Keep up to date with webkit/common/resource_type.h
    enum ResourceType {
        ResourceTypeMainFrame = 0,  // top level page
        ResourceTypeSubFrame,       // frame or iframe
        ResourceTypeStylesheet,     // a CSS stylesheet
        ResourceTypeScript,         // an external script
        ResourceTypeImage,          // an image (jpg/gif/png/etc)
        ResourceTypeFont,           // a font
        ResourceTypeOther,          // an "other" subresource.
        ResourceTypeObject,         // an object (or embed) tag for a plugin,
                                    // or a resource that a plugin requested.
        ResourceTypeMedia,          // a media resource.
        ResourceTypeWorker,         // the main resource of a dedicated worker.
        ResourceTypeSharedWorker,   // the main resource of a shared worker.
        ResourceTypePrefetch,       // an explicitly requested prefetch
        ResourceTypeFavicon,        // a favicon
        ResourceTypeXHR,            // a XMLHttpRequest
        ResourceTypePing,           // a ping request for <a ping>
        ResourceTypeServiceWorker,  // the main resource of a service worker.
    };

    ResourceType resourceType() const;

private:
    CertificateErrorControllerPrivate* d;
};

QT_END_NAMESPACE

#endif // CERTIFICATE_ERROR_CONTROLLER_H
