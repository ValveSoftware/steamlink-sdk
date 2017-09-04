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

#include "qwebenginecertificateerror.h"

QT_BEGIN_NAMESPACE

/*!
    \class QWebEngineCertificateError
    \brief The QWebEngineCertificateError class provides information about a certificate error.
    \since 5.4
    \inmodule QtWebEngineWidgets

    Provides information about a certificate error. This class is used as a parameter of
    QWebEnginePage::certificateError().
*/

class QWebEngineCertificateErrorPrivate {
public:
    QWebEngineCertificateErrorPrivate(int error, QUrl url, bool overridable, QString errorDescription);

    QWebEngineCertificateError::Error error;
    QUrl url;
    bool overridable;
    QString errorDescription;
};

QWebEngineCertificateErrorPrivate::QWebEngineCertificateErrorPrivate(int error, QUrl url, bool overridable, QString errorDescription)
    : error(QWebEngineCertificateError::Error(error))
    , url(url)
    , overridable(overridable)
    , errorDescription(errorDescription)
{ }


/*! \internal
*/
QWebEngineCertificateError::QWebEngineCertificateError(int error, QUrl url, bool overridable, QString errorDescription)
    : d_ptr(new QWebEngineCertificateErrorPrivate(error, url, overridable, errorDescription))
{ }

/*! \internal
*/
QWebEngineCertificateError::~QWebEngineCertificateError()
{
}

/*!
    \enum QWebEngineCertificateError::Error

    This enum describes the type of certificate error encountered.

    The values of this enum type match the SSL errors Chromium provides.
    QSslError::SslError values are not used directly, because the Qt error
    categories cannot be mapped to the Chromium error categories.

    \value SslPinnedKeyNotInCertificateChain The certificate did not match the built-in public keys
            pinned for the host name.
    \value CertificateCommonNameInvalid The certificate's common name did not match the host name.
    \value CertificateDateInvalid The certificate is not valid at the current date and time.
    \value CertificateAuthorityInvalid The certificate is not signed by a trusted authority.
    \value CertificateContainsErrors The certificate contains errors.
    \value CertificateNoRevocationMechanism The certificate has no mechanism for determining if it has been revoked.
    \value CertificateUnableToCheckRevocation Revocation information for the certificate is not available.
    \value CertificateRevoked The certificate has been revoked.
    \value CertificateInvalid The certificate is invalid.
    \value CertificateWeakSignatureAlgorithm The certificate is signed using a weak signature algorithm.
    \value CertificateNonUniqueName The host name specified in the certificate is not unique.
    \value CertificateWeakKey The certificate contains a weak key.
    \value CertificateNameConstraintViolation The certificate claimed DNS names that are in violation of name constraints.
    \value CertificateValidityTooLong The certificate has a validity period that is too long. (Added in Qt 5.7)
    \value CertificateTransparencyRequired Certificate Transparency was required for this connection, but the server
            did not provide CT information that complied with the policy. (Added in Qt 5.8)
*/

/*!
    Returns whether this error can be overridden and accepted.

    \sa error(), errorDescription()
*/
bool QWebEngineCertificateError::isOverridable() const
{
    const Q_D(QWebEngineCertificateError);
    return d->overridable;
}

/*!
    Returns the URL that triggered the error.

    \sa error(), errorDescription()
*/
QUrl QWebEngineCertificateError::url() const
{
    const Q_D(QWebEngineCertificateError);
    return d->url;
}

/*!
    Returns the type of the error.

    \sa errorDescription(), isOverridable()
*/
QWebEngineCertificateError::Error QWebEngineCertificateError::error() const
{
    const Q_D(QWebEngineCertificateError);
    return d->error;
}

/*!
    Returns a short localized human-readable description of the error.

    \sa error(), url(), isOverridable()
*/
QString QWebEngineCertificateError::errorDescription() const
{
    const Q_D(QWebEngineCertificateError);
    return d->errorDescription;
}

QT_END_NAMESPACE
