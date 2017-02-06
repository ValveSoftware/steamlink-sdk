/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmacinapppurchasetransaction_p.h"
#include "qmacinapppurchasebackend_p.h"

#import <StoreKit/StoreKit.h>

QT_BEGIN_NAMESPACE

QMacInAppPurchaseTransaction::QMacInAppPurchaseTransaction(SKPaymentTransaction *transaction,
                                           const TransactionStatus status,
                                           QInAppProduct *product,
                                           QMacInAppPurchaseBackend *backend)
    : QInAppTransaction(status, product, backend)
    , m_nativeTransaction(transaction)
    , m_failureReason(NoFailure)
{
    if (status == PurchaseFailed) {
        m_failureReason = ErrorOccurred;
        switch (m_nativeTransaction.error.code) {
        case SKErrorClientInvalid:
            m_errorString = QStringLiteral("Client Invalid");
            break;
        case SKErrorPaymentCancelled:
            m_errorString = QStringLiteral("Payment Cancelled");
            m_failureReason = CanceledByUser;
            break;
        case SKErrorPaymentInvalid:
            m_errorString = QStringLiteral("Payment Invalid");
            break;
        case SKErrorPaymentNotAllowed:
            m_errorString = QStringLiteral("Payment Not Allowed");
            break;
#ifdef Q_OS_IOS
        case SKErrorStoreProductNotAvailable:
            m_errorString = QStringLiteral("Store Product Not Available");
            break;
#endif
        case SKErrorUnknown:
        default:
            m_errorString = QString::fromNSString([m_nativeTransaction.error localizedDescription]);
        }
    }
}

void QMacInAppPurchaseTransaction::finalize()
{
    [[SKPaymentQueue defaultQueue] finishTransaction:m_nativeTransaction];
}

QString QMacInAppPurchaseTransaction::orderId() const
{
    return QString::fromNSString(m_nativeTransaction.transactionIdentifier);
}

QInAppTransaction::FailureReason QMacInAppPurchaseTransaction::failureReason() const
{
    return m_failureReason;
}

QString QMacInAppPurchaseTransaction::errorString() const
{
    return m_errorString;
}

QDateTime QMacInAppPurchaseTransaction::timestamp() const
{
    return QDateTime::fromNSDate(m_nativeTransaction.transactionDate);
}

QT_END_NAMESPACE

#include "moc_qmacinapppurchasetransaction_p.cpp"
