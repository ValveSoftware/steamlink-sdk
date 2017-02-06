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

#include "qmacinapppurchasebackend_p.h"
#include "qmacinapppurchaseproduct_p.h"
#include "qmacinapppurchasetransaction_p.h"

#include <QtCore/QString>

#import <StoreKit/StoreKit.h>

@interface QT_MANGLE_NAMESPACE(InAppPurchaseManager) : NSObject <SKProductsRequestDelegate, SKPaymentTransactionObserver>
{
    QMacInAppPurchaseBackend *backend;
    NSMutableArray *pendingTransactions;
}

-(void)requestProductData:(NSString *)identifier;
-(void)processPendingTransactions;

@end

@implementation QT_MANGLE_NAMESPACE(InAppPurchaseManager)

-(id)initWithBackend:(QMacInAppPurchaseBackend *)iapBackend {
    if (self = [super init]) {
        backend = iapBackend;
        pendingTransactions = [[NSMutableArray alloc] init];
        [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
        qRegisterMetaType<QMacInAppPurchaseProduct*>("QMacInAppPurchaseProduct*");
        qRegisterMetaType<QMacInAppPurchaseTransaction*>("QMacInAppPurchaseTransaction*");
    }
    return self;
}

-(void)dealloc
{
    [[SKPaymentQueue defaultQueue] removeTransactionObserver:self];
    [pendingTransactions release];
    [super dealloc];
}

-(void)requestProductData:(NSString *)identifier
{
    NSSet *productId = [NSSet setWithObject:identifier];
    SKProductsRequest *productsRequest = [[SKProductsRequest alloc] initWithProductIdentifiers:productId];
    productsRequest.delegate = self;
    [productsRequest start];
}

-(void)processPendingTransactions
{
    NSMutableArray *registeredTransactions = [NSMutableArray array];

    for (SKPaymentTransaction *transaction in pendingTransactions) {
        QInAppTransaction::TransactionStatus status = [QT_MANGLE_NAMESPACE(InAppPurchaseManager) statusFromTransaction:transaction];

        QMacInAppPurchaseProduct *product = backend->registeredProductForProductId(QString::fromNSString(transaction.payment.productIdentifier));

        if (product) {
            //It is possible that the product doesn't exist yet (because of previous restores).
            QMacInAppPurchaseTransaction *qtTransaction = new QMacInAppPurchaseTransaction(transaction, status, product);
            if (qtTransaction->thread() != backend->thread()) {
                qtTransaction->moveToThread(backend->thread());
                QMetaObject::invokeMethod(backend, "setParentToBackend", Qt::AutoConnection, Q_ARG(QObject*, qtTransaction));
            }
            [registeredTransactions addObject:transaction];
            QMetaObject::invokeMethod(backend, "registerTransaction", Qt::AutoConnection, Q_ARG(QMacInAppPurchaseTransaction*, qtTransaction));
        }
    }

    //Remove registeredTransactions from pendingTransactions
    [pendingTransactions removeObjectsInArray:registeredTransactions];
}


//SKProductsRequestDelegate
-(void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response
{
    NSArray *products = response.products;
    SKProduct *product = [products count] == 1 ? [[products firstObject] retain] : nil;

    if (product == nil) {
        //Invalid product ID
        NSString *invalidId = [response.invalidProductIdentifiers firstObject];
        QMetaObject::invokeMethod(backend, "registerQueryFailure", Qt::AutoConnection, Q_ARG(QString, QString::fromNSString(invalidId)));
    } else {
        //Valid product query
        //Create a QMacInAppPurchaseProduct
        QMacInAppPurchaseProduct *validProduct = new QMacInAppPurchaseProduct(product, backend->productTypeForProductId(QString::fromNSString([product productIdentifier])));
        if (validProduct->thread() != backend->thread()) {
            validProduct->moveToThread(backend->thread());
            QMetaObject::invokeMethod(backend, "setParentToBackend", Qt::AutoConnection, Q_ARG(QObject*, validProduct));
        }
        QMetaObject::invokeMethod(backend, "registerProduct", Qt::AutoConnection, Q_ARG(QMacInAppPurchaseProduct*, validProduct));
    }

    [request release];
}

+(QInAppTransaction::TransactionStatus)statusFromTransaction:(SKPaymentTransaction *)transaction
{
    QInAppTransaction::TransactionStatus status;
    switch (transaction.transactionState) {
        case SKPaymentTransactionStatePurchasing:
            //Ignore the purchasing state as it's not really a transaction
            //And its important that it doesn't need to be finalized as
            //Calling finishTransaction: on a transaction that is
            //in the SKPaymentTransactionStatePurchasing state throws an exception
            status = QInAppTransaction::Unknown;
            break;
        case SKPaymentTransactionStatePurchased:
            status = QInAppTransaction::PurchaseApproved;
            break;
        case SKPaymentTransactionStateFailed:
            status = QInAppTransaction::PurchaseFailed;
            break;
        case SKPaymentTransactionStateRestored:
            status = QInAppTransaction::PurchaseRestored;
            break;
        default:
            status = QInAppTransaction::Unknown;
            break;
    }
    return status;
}

//SKPaymentTransactionObserver
- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
{
    Q_UNUSED(queue);
    for (SKPaymentTransaction *transaction in transactions) {
        //Create QMacInAppPurchaseTransaction
        QInAppTransaction::TransactionStatus status = [QT_MANGLE_NAMESPACE(InAppPurchaseManager) statusFromTransaction:transaction];

        if (status == QInAppTransaction::Unknown)
            continue;

        QMacInAppPurchaseProduct *product = backend->registeredProductForProductId(QString::fromNSString(transaction.payment.productIdentifier));

        if (product) {
            //It is possible that the product doesn't exist yet (because of previous restores).
            QMacInAppPurchaseTransaction *qtTransaction = new QMacInAppPurchaseTransaction(transaction, status, product);
            if (qtTransaction->thread() != backend->thread()) {
                qtTransaction->moveToThread(backend->thread());
                QMetaObject::invokeMethod(backend, "setParentToBackend", Qt::AutoConnection, Q_ARG(QObject*, qtTransaction));
            }
            QMetaObject::invokeMethod(backend, "registerTransaction", Qt::AutoConnection, Q_ARG(QMacInAppPurchaseTransaction*, qtTransaction));
        } else {
            //Add the transaction to the pending transactions list
            [pendingTransactions addObject:transaction];
        }
    }
}

@end


QT_BEGIN_NAMESPACE

QMacInAppPurchaseBackend::QMacInAppPurchaseBackend(QObject *parent)
    : QInAppPurchaseBackend(parent)
    , m_iapManager(0)
{
}

QMacInAppPurchaseBackend::~QMacInAppPurchaseBackend()
{
    [m_iapManager release];
}

void QMacInAppPurchaseBackend::initialize()
{
    m_iapManager = [[QT_MANGLE_NAMESPACE(InAppPurchaseManager) alloc] initWithBackend:this];
    emit QInAppPurchaseBackend::ready();
}

bool QMacInAppPurchaseBackend::isReady() const
{
    if (m_iapManager)
        return true;
    return false;
}

void QMacInAppPurchaseBackend::queryProduct(QInAppProduct::ProductType productType, const QString &identifier)
{
    Q_UNUSED(productType)

    if (m_productTypeForPendingId.contains(identifier)) {
        qWarning("Product query already pending for %s", qPrintable(identifier));
        return;
    }

    m_productTypeForPendingId[identifier] = productType;

    [m_iapManager requestProductData:(identifier.toNSString())];
}

void QMacInAppPurchaseBackend::restorePurchases()
{
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

void QMacInAppPurchaseBackend::setPlatformProperty(const QString &propertyName, const QString &value)
{
    Q_UNUSED(propertyName);
    Q_UNUSED(value);
}

void QMacInAppPurchaseBackend::registerProduct(QMacInAppPurchaseProduct *product)
{
    QHash<QString, QInAppProduct::ProductType>::iterator it = m_productTypeForPendingId.find(product->identifier());
    Q_ASSERT(it != m_productTypeForPendingId.end());

    m_registeredProductForId[product->identifier()] = product;
    emit productQueryDone(product);
    m_productTypeForPendingId.erase(it);
    [m_iapManager processPendingTransactions];
}

void QMacInAppPurchaseBackend::registerQueryFailure(const QString &productId)
{
    QHash<QString, QInAppProduct::ProductType>::iterator it = m_productTypeForPendingId.find(productId);
    Q_ASSERT(it != m_productTypeForPendingId.end());

    emit QInAppPurchaseBackend::productQueryFailed(it.value(), it.key());
    m_productTypeForPendingId.erase(it);
}

void QMacInAppPurchaseBackend::registerTransaction(QMacInAppPurchaseTransaction *transaction)
{
    emit QInAppPurchaseBackend::transactionReady(transaction);
}

QInAppProduct::ProductType QMacInAppPurchaseBackend::productTypeForProductId(const QString &productId)
{
    return m_productTypeForPendingId[productId];
}

QMacInAppPurchaseProduct *QMacInAppPurchaseBackend::registeredProductForProductId(const QString &productId)
{
    return m_registeredProductForId[productId];
}

void QMacInAppPurchaseBackend::setParentToBackend(QObject *object)
{
    object->setParent(this);
}

QT_END_NAMESPACE

#include "moc_qmacinapppurchasebackend_p.cpp"
