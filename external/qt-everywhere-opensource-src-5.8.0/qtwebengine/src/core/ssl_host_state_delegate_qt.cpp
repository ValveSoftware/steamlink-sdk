/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ssl_host_state_delegate_qt.h"

#include "type_conversion.h"

namespace QtWebEngineCore {

// Mirrors implementation in aw_ssl_host_state_delegate.cc

static net::SHA256HashValue getChainFingerprint256(const net::X509Certificate &cert)
{
    net::SHA256HashValue fingerprint =
            net::X509Certificate::CalculateChainFingerprint256(cert.os_cert_handle(), cert.GetIntermediateCertificates());
    return fingerprint;
}

CertPolicy::CertPolicy()
{
}

CertPolicy::~CertPolicy()
{
}

bool CertPolicy::Check(const net::X509Certificate& cert, net::CertStatus error) const
{
    net::SHA256HashValue fingerprint = getChainFingerprint256(cert);
    auto allowed_iter = m_allowed.find(fingerprint);
    if ((allowed_iter != m_allowed.end()) && (allowed_iter->second & error) && ((allowed_iter->second & error) == error))
        return true;
    return false;
}

void CertPolicy::Allow(const net::X509Certificate& cert, net::CertStatus error)
{
    net::SHA256HashValue fingerprint = getChainFingerprint256(cert);
    m_allowed[fingerprint] |= error;
}

SSLHostStateDelegateQt::SSLHostStateDelegateQt()
{
}

SSLHostStateDelegateQt::~SSLHostStateDelegateQt()
{
}

void SSLHostStateDelegateQt::AllowCert(const std::string &host, const net::X509Certificate &cert, net::CertStatus error)
{
    m_certPolicyforHost[host].Allow(cert, error);
}

// Clear all allow preferences.
void SSLHostStateDelegateQt::Clear()
{
    m_certPolicyforHost.clear();
}

// Queries whether |cert| is allowed for |host| and |error|. Returns true in
// |expired_previous_decision| if a previous user decision expired immediately
// prior to this query, otherwise false.
content::SSLHostStateDelegate::CertJudgment SSLHostStateDelegateQt::QueryPolicy(
                                                       const std::string &host, const net::X509Certificate &cert,
                                                       net::CertStatus error,bool *expired_previous_decision)
{
    return m_certPolicyforHost[host].Check(cert, error) ? SSLHostStateDelegate::ALLOWED : SSLHostStateDelegate::DENIED;
}

// Records that a host has run insecure content.
void SSLHostStateDelegateQt::HostRanInsecureContent(const std::string &host, int pid)
{
}

// Returns whether the specified host ran insecure content.
bool SSLHostStateDelegateQt::DidHostRunInsecureContent(const std::string &host, int pid) const
{
    return false;
}

// Revokes all SSL certificate error allow exceptions made by the user for
// |host|.
void SSLHostStateDelegateQt::RevokeUserAllowExceptions(const std::string &host)
{
    m_certPolicyforHost.erase(host);
}

// Returns whether the user has allowed a certificate error exception for
// |host|. This does not mean that *all* certificate errors are allowed, just
// that there exists an exception. To see if a particular certificate and
// error combination exception is allowed, use QueryPolicy().
bool SSLHostStateDelegateQt::HasAllowException(const std::string &host) const
{
    auto policy_iterator = m_certPolicyforHost.find(host);
    return policy_iterator != m_certPolicyforHost.end() &&
           policy_iterator->second.HasAllowException();
}


} // namespace QtWebEngineCore
