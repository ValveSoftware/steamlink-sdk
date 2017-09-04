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

#include "certificate_error_controller.h"
#include "certificate_error_controller_p.h"

#include <net/base/net_errors.h>
#include <net/cert/x509_certificate.h>
#include <net/ssl/ssl_info.h>
#include <ui/base/l10n/l10n_util.h>
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "type_conversion.h"

QT_BEGIN_NAMESPACE

using namespace QtWebEngineCore;

ASSERT_ENUMS_MATCH(CertificateErrorController::SslPinnedKeyNotInCertificateChain, net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateCommonNameInvalid, net::ERR_CERT_BEGIN)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateCommonNameInvalid, net::ERR_CERT_COMMON_NAME_INVALID)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateDateInvalid, net::ERR_CERT_DATE_INVALID)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateAuthorityInvalid, net::ERR_CERT_AUTHORITY_INVALID)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateContainsErrors, net::ERR_CERT_CONTAINS_ERRORS)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateUnableToCheckRevocation, net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateRevoked, net::ERR_CERT_REVOKED)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateInvalid, net::ERR_CERT_INVALID)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateWeakSignatureAlgorithm, net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateNonUniqueName, net::ERR_CERT_NON_UNIQUE_NAME)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateWeakKey, net::ERR_CERT_WEAK_KEY)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateNameConstraintViolation, net::ERR_CERT_NAME_CONSTRAINT_VIOLATION)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateValidityTooLong, net::ERR_CERT_VALIDITY_TOO_LONG)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateTransparencyRequired, net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED)
ASSERT_ENUMS_MATCH(CertificateErrorController::CertificateErrorEnd, net::ERR_CERT_END)

void CertificateErrorControllerPrivate::accept(bool accepted)
{
    callback.Run(accepted ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE : content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

CertificateErrorControllerPrivate::CertificateErrorControllerPrivate(int cert_error,
                                                                     const net::SSLInfo& ssl_info,
                                                                     const GURL &request_url,
                                                                     content::ResourceType resource_type,
                                                                     bool _overridable,
                                                                     bool strict_enforcement,
                                                                     const base::Callback<void(content::CertificateRequestResultType)>& cb
                                                                    )
    : certError(CertificateErrorController::CertificateError(cert_error))
    , requestUrl(toQt(request_url))
    , resourceType(CertificateErrorController::ResourceType(resource_type))
    , overridable(_overridable)
    , strictEnforcement(strict_enforcement)
    , callback(cb)
{
    if (ssl_info.cert.get()) {
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
        return getQStringForMessageId(IDS_CERT_ERROR_SUMMARY_PINNING_FAILURE_DETAILS);
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
    case CertificateValidityTooLong:
        return getQStringForMessageId(IDS_CERT_ERROR_VALIDITY_TOO_LONG_DESCRIPTION);
    case CertificateTransparencyRequired:
        return getQStringForMessageId(IDS_CERT_ERROR_CERTIFICATE_TRANSPARENCY_REQUIRED_DESCRIPTION);
    case CertificateUnableToCheckRevocation: // Deprecated in Chromium.
    default:
        break;
    }

    return getQStringForMessageId(IDS_CERT_ERROR_UNKNOWN_ERROR_DESCRIPTION);
}

QT_END_NAMESPACE
