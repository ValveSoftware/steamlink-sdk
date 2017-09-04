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

#include "qmacinapppurchaseproduct_p.h"
#include "qmacinapppurchasebackend_p.h"

#import <StoreKit/StoreKit.h>

QT_BEGIN_NAMESPACE

static NSString *localizedPrice(SKProduct *product)
{
    NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
    [numberFormatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
    [numberFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
    [numberFormatter setLocale:product.priceLocale];
    NSString *formattedString = [numberFormatter stringFromNumber:product.price];
    [numberFormatter release];
    return formattedString;
}

QMacInAppPurchaseProduct::QMacInAppPurchaseProduct(SKProduct *product,
                                                   ProductType productType,
                                                   QMacInAppPurchaseBackend *backend)
    : QInAppProduct(QString::fromNSString(localizedPrice(product)),
                    QString::fromNSString([product localizedTitle]),
                    QString::fromNSString([product localizedDescription]),
                    productType,
                    QString::fromNSString([product productIdentifier]),
                    backend)
    , m_nativeProduct(product)
{
}

void QMacInAppPurchaseProduct::purchase()
{
    SKPayment *payment = [SKPayment paymentWithProduct:m_nativeProduct];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
}

QT_END_NAMESPACE

#include "moc_qmacinapppurchaseproduct_p.cpp"
