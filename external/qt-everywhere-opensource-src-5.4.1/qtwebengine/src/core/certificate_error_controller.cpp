/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "certificate_error_controller.h"
#include "certificate_error_controller_p.h"

#include <net/cert/x509_certificate.h>
#include <net/ssl/ssl_info.h>
#include <ui/base/l10n/l10n_util.h>
#include "chrome/grit/generated_resources.h"
#include "type_conversion.h"

void CertificateErrorControllerPrivate::accept(bool accepted)
{
    callback.Run(accepted);
}

CertificateErrorControllerPrivate::CertificateErrorControllerPrivate(int cert_error,
                                                                     const net::SSLInfo& ssl_info,
                                                                     const GURL &request_url,
                                                                     ResourceType::Type resource_type,
                                                                     bool _overridable,
                                                                     bool strict_enforcement,
                                                                     const base::Callback<void(bool)>& cb
                                                                    )
    : certError(CertificateErrorController::CertificateError(cert_error))
    , requestUrl(toQt(request_url))
    , resourceType(CertificateErrorController::ResourceType(resource_type))
    , overridable(_overridable)
    , strictEnforcement(strict_enforcement)
    , callback(cb)
{
    if (ssl_info.cert) {
        validStart = toQt(ssl_info.cert->valid_start());
        validExpiry = toQt(ssl_info.cert->valid_expiry());
    }
}

CertificateErrorController::CertificateErrorController(CertificateErrorControllerPrivate *p) : d(p)
{
}

CertificateErrorController::~CertificateErrorController()
{
    delete d;
    d = 0;
}

CertificateErrorController::CertificateError CertificateErrorController::error() const
{
    return d->certError;
}

QUrl CertificateErrorController::url() const
{
    return d->requestUrl;
}

bool CertificateErrorController::overridable() const
{
    return d->overridable;
}

bool CertificateErrorController::strictEnforcement() const
{
    return d->strictEnforcement;
}

void CertificateErrorController::accept(bool accepted)
{
    d->accept(accepted);
}

CertificateErrorController::ResourceType CertificateErrorController::resourceType() const
{
    return d->resourceType;
}

static QString getQStringForMessageId(int message_id) {
    base::string16 string = l10n_util::GetStringUTF16(message_id);
    return toQt(string);
}

QString CertificateErrorController::errorString() const
{
    // Try to use chromiums translation of the error strings, though not all are
    // consistently described and we need to use versions that does not contain HTML
    // formatted text.
    switch (d->certError) {
    case SslPinnedKeyNotInCertificateChain:
        return getQStringForMessageId(IDS_ERRORPAGES_SUMMARY_PINNING_FAILURE);
    case CertificateCommonNameInvalid:
        return getQStringForMessageId(IDS_CERT_ERROR_COMMON_NAME_INVALID_DESCRIPTION);
    case CertificateDateInvalid:
        if (QDateTime::currentDateTime() > d->validExpiry)
            return getQStringForMessageId(IDS_CERT_ERROR_EXPIRED_DESCRIPTION);
        else
            return getQStringForMessageId(IDS_CERT_ERROR_NOT_YET_VALID_DESCRIPTION);
    case CertificateAuthorityInvalid:
        return getQStringForMessageId(IDS_CERT_ERROR_AUTHORITY_INVALID_DESCRIPTION);
    case CertificateContainsErrors:
        return getQStringForMessageId(IDS_CERT_ERROR_CONTAINS_ERRORS_DESCRIPTION);
    case CertificateNoRevocationMechanism:
        return getQStringForMessageId(IDS_CERT_ERROR_NO_REVOCATION_MECHANISM_DETAILS);
    case CertificateUnableToCheckRevocation:
        return getQStringForMessageId(IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_DETAILS);
    case CertificateRevoked:
        return getQStringForMessageId(IDS_CERT_ERROR_REVOKED_CERT_DESCRIPTION);
    case CertificateInvalid:
        return getQStringForMessageId(IDS_CERT_ERROR_INVALID_CERT_DESCRIPTION);
    case CertificateWeakSignatureAlgorithm:
        return getQStringForMessageId(IDS_CERT_ERROR_WEAK_SIGNATURE_ALGORITHM_DESCRIPTION);
    case CertificateNonUniqueName:
        return getQStringForMessageId(IDS_PAGE_INFO_SECURITY_TAB_NON_UNIQUE_NAME);
    case CertificateWeakKey:
        return getQStringForMessageId(IDS_CERT_ERROR_WEAK_KEY_DESCRIPTION);
    case CertificateNameConstraintViolation:
        return getQStringForMessageId(IDS_CERT_ERROR_NAME_CONSTRAINT_VIOLATION_DESCRIPTION);
    default:
        break;
    }

    return getQStringForMessageId(IDS_CERT_ERROR_UNKNOWN_ERROR_DESCRIPTION);
}
