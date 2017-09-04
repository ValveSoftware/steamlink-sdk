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

#ifndef QQUICKWEBENGINECERTIFICATEERROR_P_H
#define QQUICKWEBENGINECERTIFICATEERROR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include "qquickwebengineview_p.h"

QT_BEGIN_NAMESPACE

class QQuickWebEngineCertificateErrorPrivate;
class CertificateErrorController;

class Q_WEBENGINE_EXPORT QQuickWebEngineCertificateError : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url CONSTANT FINAL)
    Q_PROPERTY(Error error READ error CONSTANT FINAL)
    Q_PROPERTY(QString description READ description CONSTANT FINAL)
    Q_PROPERTY(bool overridable READ overridable CONSTANT FINAL)

public:

    // Keep this identical to CertificateErrorController::CertificateError, or add mapping layer.
    enum Error {
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
    };
    Q_ENUM(Error)

    QQuickWebEngineCertificateError(const QSharedPointer<CertificateErrorController> &controller, QObject *parent = 0);
    ~QQuickWebEngineCertificateError();

    Q_INVOKABLE void defer();
    Q_INVOKABLE void ignoreCertificateError();
    Q_INVOKABLE void rejectCertificate();
    QUrl url() const;
    Error error() const;
    QString description() const;
    bool overridable() const;
    bool deferred() const;
    bool answered() const;

private:
    Q_DISABLE_COPY(QQuickWebEngineCertificateError)
    Q_DECLARE_PRIVATE(QQuickWebEngineCertificateError)
    QScopedPointer<QQuickWebEngineCertificateErrorPrivate> d_ptr;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineCertificateError)

#endif // QQUICKWEBENGINECERTIFICATEERROR_P_H
