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

#include "qinappstore.h"
#include "qinappstore_p.h"
#include "qinapppurchasebackend_p.h"
#include "qinapppurchasebackendfactory_p.h"

namespace
{
class IAPRegisterMetaTypes
{
public:
    IAPRegisterMetaTypes()
    {
        qRegisterMetaType<QInAppProduct::ProductType>("QInAppProduct::ProductType");
    }
} _registerIAPMetaTypes;
}

QT_BEGIN_NAMESPACE

/*!
    \class QInAppStore
    \inmodule QtPurchasing
    \brief The main entry point for managing in-app purchases

    QInAppStore is used for managing in-app purchases in your application in a
    cross-platform way.

    \section1 Using the QInAppStore
    In general there are two steps to completing an in-app purchase using the
    API:

    \section2 Initialize the store
    Upon start-up of your application, connect all signals in QInAppStore to
    related slots in your own QObject. Then use the registerProduct() function
    to register the ID of each product you expect to find registered in the
    external store, as well as its type.

    Registering a product is asynchronous, and will at some point yield one of
    the following two signals:
    1. productRegistered() if the product was found in the external store with
       a matching type.
    2. productUnknown() if the product was not found in the external store with
       the type you specified.

    In addition, a transactionReady() signal may be emitted for any existing
    transaction which has not yet been finalized. At this point, you should
    check if the transaction has previously been registered. If it hasn't,
    register it right away. Finally, call QInAppTransaction::finalize() on
    the transaction.

    \section2 Complete a purchase
    Once the items have been successfully registered in the store, you can
    purchase them. Get the previously registered QInAppProduct using
    registeredProduct() and call QInAppProduct::purchase(). This call is
    also asynchronous.

    At some point later on, the transactionReady() signal will be emitted for
    the purchase. Check QInAppTransaction::status() to see if the purchase was
    completed successfully. If it was, then you must save the information about
    the purchase in a safe way, so that the application can restore it later.

    When you are done, call QInAppTransaction::finalize(), regardless of its
    status. Transactions which are not finalized will be emitted again the next
    time your application calls registerProduct() for the same product.

    \note Please mind that QInAppStore does not save the purchased
    state of items in the store for you. The application should store this
    information in a safe way upon receiving the transactionReady() signal,
    before calling QInAppTransaction::finalize().

    \section1 Types of purchases
    There are two types of purchases supported by QInAppStore:
    QInAppProduct::Consumable and QInAppProduct::Unlockable. The former will be
    consumed when the transaction is completed and QInAppTransaction::finalize()
    is called, meaning that it can be purchased again, any number of times.
    Unlockable items can only be purchased once.

    Consumable products are temporary and can be purchased multiple times.
    Examples could be a day-ticket on the bus or a magic sword in a computer game.
    Note that when purchasing the same product multiple times, you should call
    QInAppTransaction::finalize() on each transaction before you can purchase the
    same product again.

    Unlockable products are products that a user will buy once, and the purchase
    of these items will be persistent. It can typically be used for things like
    unlocking content or functionality in the application.

    \section1 Restoring purchases
    If your application has unlockable products, and does not store the purchase
    states of these products in a way which makes it possible to restore them
    when the user reinstalls the application, you should provide a way for the
    user to restore the purchases manually.

    Call the restorePurchases() function to begin this process. Granted that
    the remote store supports it, you will then at some point get transactionReady()
    for each unlockable item which has previously been purchased by the current user.

    Save the purchase state of each product and call QInAppTransaction::finalize() as
    you would for a regular purchase.

    Since restorePurchases() may, on some platforms, cause the user to be prompted for
    their password, it should usually be called as a reaction to user input. For instance
    applications can have a button in the UI which will trigger restorePurchases() and
    which users can hit manually if they have reinstalled the application (or installed
    it on a new device) and need to unlock the features that they have previously paid
    for.

    \note This depends on support for this functionality in the remote store. If
    the remote store does not save the purchase state of unlockable products for
    you, the call will yield no transactionReady() signals, as if no products have
    been purchased. Both the Android and OS X / iOS backends support restoring unlockable
    products.

*/

/*!
 * Constructs a QInAppStore with the given \a parent.
 */
QInAppStore::QInAppStore(QObject *parent)
    : QObject(parent)
{
    d = QSharedPointer<QInAppStorePrivate>(new QInAppStorePrivate);
    setupBackend();
}

/*!
 * Destroys the QInAppStore.
 */
QInAppStore::~QInAppStore()
{
}

/*!
 * \internal
 */
void QInAppStore::setupBackend()
{
    d->backend = QInAppPurchaseBackendFactory::create();
    d->backend->setStore(this);

    connect(d->backend, SIGNAL(ready()),
            this, SLOT(registerPendingProducts()));
    connect(d->backend, SIGNAL(transactionReady(QInAppTransaction *)),
            this, SIGNAL(transactionReady(QInAppTransaction *)));
    connect(d->backend, SIGNAL(productQueryFailed(QInAppProduct::ProductType,QString)),
            this, SIGNAL(productUnknown(QInAppProduct::ProductType,QString)));
    connect(d->backend, SIGNAL(productQueryDone(QInAppProduct *)),
            this, SLOT(registerProduct(QInAppProduct*)));
}

/*!
 * \internal
 */
void QInAppStore::registerProduct(QInAppProduct *product)
{
    d->registeredProducts[product->identifier()] = product;
    emit productRegistered(product);
}

/*!
 * \internal
 *
 * Called when the backend is finished initialized and will create products which were
 * registered while the backend was still working.
 */
void QInAppStore::registerPendingProducts()
{
    QList<QInAppPurchaseBackend::Product> products;
    products.reserve(d->pendingProducts.size());

    QHash<QString, QInAppProduct::ProductType>::const_iterator it;
    for (it = d->pendingProducts.constBegin(); it != d->pendingProducts.constEnd(); ++it)
        products.append(QInAppPurchaseBackend::Product(it.value(), it.key()));
    d->pendingProducts.clear();

    d->backend->queryProducts(products);
    if (d->pendingRestorePurchases)
        restorePurchases();
}

/*!
 * Requests existing purchases of unlockable items and will yield a transactionReady()
 * signal for each unlockable product that the remote store confirms have previously been
 * purchased by the current user.
 *
 * This function can typically be used for restoring unlockable products when the application
 * has been reinstalled and lost the saved purchase states.
 *
 * \note Calling this function may prompt the user for their password on some platforms.
 */
void QInAppStore::restorePurchases()
{
    if (d->backend->isReady()) {
        d->pendingRestorePurchases = false;
        d->backend->restorePurchases();
    } else {
        d->pendingRestorePurchases = true;
    }
}

/*!
 * Sets the platform specific property given by \a propertyName to \a value. This can be used
 * to pass information to the platform implementation. The properties will be silently ignored
 * on other platforms.
 *
 * Currently, the only supported platform property is "AndroidPublicKey" which is used by the Android
 * backend to verify purchases. If it is not set, purchases will be accepted with no verification.
 * (You can also do the verification manually by getting the signature from the QInAppTransaction object
 * for the purchase.) For more information, see
 * \l{http://developer.android.com/google/play/billing/billing_integrate.html#billing-security}
 * {the Android documentation for billing security}.
 *
 */
void QInAppStore::setPlatformProperty(const QString &propertyName, const QString &value)
{
    d->backend->setPlatformProperty(propertyName, value);
}

/*!
 * Registers a product identified by \a identifier and with the given \a productType.
 * The \a identifier must match the identifier of the product in the remote store. If
 * the remote store differentiates between consumable and unlockable products, the
 * \a productType must also match this.
 *
 * Calling this function will asynchronously yield either a productRegistered() or a
 * productUnknown() signal. It may also yield a transactionReady() signal if there is
 * a pending transaction for the product which has not yet been finalized.
 */
void QInAppStore::registerProduct(QInAppProduct::ProductType productType, const QString &identifier)
{
    if (!d->backend->isReady()) {
        d->pendingProducts[identifier] = productType;
        if (!d->hasCalledInitialize) {
            d->hasCalledInitialize = true;
            d->backend->initialize();
        }
    } else {
        d->backend->queryProduct(productType, identifier);
    }
}

/*!
 * Returns the previously registered product uniquely known by the \a identifier.
 */
QInAppProduct *QInAppStore::registeredProduct(const QString &identifier) const
{
    return d->registeredProducts.value(identifier);
}

/*!
 * \fn QInAppStore::productRegistered(QInAppProduct *product)
 *
 * This signal is emitted when information about a \a product has been collected from the
 * remote store. It is emitted as a reaction to a registerProduct() call for the same
 * product.
 *
 * \sa productUnknown()
 */

/*! \fn QInAppStore::productUnknown(QInAppProduct::ProductType productType, const QString &identifier)
 *
 * This signal is emitted when the product named \a identifier was registered using registerProduct()
 * and matching information could not be provided by the remote store. The \a productType matches
 * the product type which was originally passed to registerProduct().
 *
 * \sa productRegistered()
 */

/*!
 * \fn QInAppStore::transactionReady(QInAppTransaction *transaction)
 *
 * This signal is emitted whenever there is a \a transaction which needs to be finalized.
 * It is emitted either when a purchase request has been made for a product, when restorePurchases()
 * has been called and the product was previously purchased, or when registerProduct() was called
 * for a product and there was a pending transaction for the product which had not yet been finalized.
 */

QT_END_NAMESPACE
