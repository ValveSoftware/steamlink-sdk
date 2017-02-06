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

#include "qinappproductqmltype_p.h"
#include "qinappstoreqmltype_p.h"
#include <QtPurchasing/qinapptransaction.h>
#include <QtPurchasing/qinappstore.h>
#include <QtCore/qcoreevent.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Product
    \inqmlmodule QtPurchasing
    \since QtPurchasing 1.0
    \ingroup qtpurchasing
    \brief A product for in-app purchasing.

    Product contains information about a product in the external market place. Once
    the product's \l identifier and \l type are set, the product will be queried from
    the external market place. Properties such as \l price will then be set, and
    it will be possible to purchase the product. The \l status property holds information
    on the registration process.

    \note It is not possible to change the identifier and type once they have both been set
    and the product has been registered.
*/

QInAppProductQmlType::QInAppProductQmlType(QObject *parent)
    : QObject(parent)
    , m_status(Uninitialized)
    , m_type(QInAppProductQmlType::ProductType(-1))
    , m_componentComplete(false)
    , m_store(0)
    , m_product(0)
{
}

/*!
  \qmlproperty object QtPurchasing::Product::store

  This property holds the store containing the product. When the product is created as
  a child of the store, this is set automatically to the parent, as in the following
  example:

  \qml
  Store {
      Product {
        // No need to set the store explicitly here, as it will automatically be
        // bound to the parent
        identifier: "myConsumableProduct"
        type: Product.Consumable
      }
      Product {
        // No need to set the store explicitly here, as it will automatically be
        // bound to the parent
        identifier: "myUnlockableProduct"
        type: Product.Unlockable
      }
  }
  \endqml

  However, in some advanced use cases, for example when products are created based on
  a model, it's also possible to create the product anywhere in the QML document
  and set the store explicitly, like in the following example:

  \code
  ListModel {
      id: productModel
      ListElement {
          productIdentifier: "myConsumableProduct"
          productType: Product.Consumable
      }
      ListElement {
          productIdentifier: "myUnlockableProduct"
          productType: Product.Unlockable
      }
  }

  Store {
      id: myStore
  }

  Instantiator {
      model: productModel
      delegate: Product {
                      identifier: productIdentifier
                      type: productType
                      store: myStore
                }
  }
  \endcode
 */
void QInAppProductQmlType::setStore(QInAppStoreQmlType *store)
{
    if (m_store == store)
        return;

    if (m_store != 0)
        m_store->store()->disconnect(this);

    m_store = store;
    connect(m_store->store(), SIGNAL(productRegistered(QInAppProduct*)),
            this, SLOT(handleProductRegistered(QInAppProduct *)));
    connect(m_store->store(), SIGNAL(productUnknown(QInAppProduct::ProductType,QString)),
            this, SLOT(handleProductUnknown(QInAppProduct::ProductType,QString)));
    connect(m_store->store(), SIGNAL(transactionReady(QInAppTransaction*)),
            this, SLOT(handleTransaction(QInAppTransaction*)));

    updateProduct();

    emit storeChanged();
}

QInAppStoreQmlType *QInAppProductQmlType::store() const
{
    return m_store;
}

void QInAppProductQmlType::componentComplete()
{
    if (!m_componentComplete) {
        m_componentComplete = true;
        updateProduct();
    }
}

/*!
  \qmlproperty string QtPurchasing::Product::identifier
  This property holds the identifier of the product in the external market place. It must match the
  identifier used to register the product externally before-hand.

  When both the identifier and \l type is set, the product is queried from the external market place,
  and its other properties are updated asynchronously. At this point, the identifier and type
  can no longer be changed.

  The following example queries an unlockable product named "myUnlockableProduct" from the external
  market place.
  \qml
  Store {
      Product {
        identifier: "myUnlockableProduct"
        type: Product.Unlockable

        // ...
      }
  }
  \endqml
*/
void QInAppProductQmlType::setIdentifier(const QString &identifier)
{
    if (m_identifier == identifier)
        return;

    if (m_status != Uninitialized) {
        qWarning("A product's identifier cannot be changed once the product has been initialized.");
        return;
    }

    m_identifier = identifier;
    if (m_componentComplete)
        updateProduct();
    emit identifierChanged();
}

void QInAppProductQmlType::updateProduct()
{
    if (m_store == 0)
        return;

    Status oldStatus = m_status;
    QInAppProduct *product = 0;
    if (m_identifier.isEmpty() || m_type == QInAppProductQmlType::ProductType(-1)) {
        m_status = Uninitialized;
    } else {
        product = m_store->store()->registeredProduct(m_identifier);
        if (product != 0 && product == m_product)
            return;

        if (product == 0) {
            m_status = PendingRegistration;
            m_store->store()->registerProduct(QInAppProduct::ProductType(m_type), m_identifier);
        } else if (product->productType() != QInAppProduct::ProductType(m_type)) {
            qWarning("Product registered multiple times with different product types.");
            product = 0;
            m_status = Uninitialized;
        } else {
            m_status = Registered;
        }
    }

    setProduct(product);
    if (oldStatus != m_status)
        emit statusChanged();
}

/*!
  \qmlmethod QtPurchasing::Product::resetStatus()

  Resets the \l status of this product and retries querying it from the external
  market place.

  This method can be used when querying the product failed for some reason
  (such as network timeouts).

  \since QtPurchasing 1.0.2
*/
void QInAppProductQmlType::resetStatus()
{
    updateProduct();
}

QString QInAppProductQmlType::identifier() const
{
    return m_identifier;
}

/*!
  \qmlproperty string QtPurchasing::Product::type
  This property holds the type of the product in the external market place.

  It can hold one of the following values:
  \list
  \li Product.Consumable The product is consumable and can be purchased more than once
  by the same user, granted that the transaction for the previous purchase has been finalized.
  \li Product.Unlockable The product can only be purchased once per user. If the application
  is uninstalled and reinstalled on the device (or installed on a new device by the same user),
  purchases of unlockable products can be restored using the store's
  \l{QtPurchasing::Store::restorePurchases()}{restorePurchases()} method.
  \endlist

  When both the \l identifier and type is set, the product is queried from the external market place,
  and its other properties are updated asynchronously. At this point, the identifier and type
  can no longer be changed.

  The following example queries an unlockable product named "myUnlockableProduct" from the external
  market place.
  \qml
  Store {
      Product {
        identifier: "myUnlockableProduct"
        type: Product.Unlockable

        // ...
      }
  }
  \endqml
*/
void QInAppProductQmlType::setType(QInAppProductQmlType::ProductType type)
{
    if (m_type == type)
        return;

    if (m_status != Uninitialized) {
        qWarning("A product's type cannot be changed once the product has been initialized.");
        return;
    }

    m_type = type;
    if (m_componentComplete)
        updateProduct();

    emit typeChanged();
}

QInAppProductQmlType::ProductType QInAppProductQmlType::type() const
{
    return m_type;
}

/*!
  \qmlproperty enumeration QtPurchasing::Product::status
  This property holds the current status of the product in the registration sequence.

  \list
  \li Product.Uninitialized - This is initial status, before the identifier property has been set.
  \li Product.PendingRegistration - Indicates that the product is currently being queried from the
  external market place. The product gets this status when its identifier is set.
  \li Product.Registered - Indicates that the product was successfully found in the external market
  place. Its price can now be queried and the product can be purchased.
  \li Product.Unknown - The product could not be found in the external market place. This could
  for example be due to misspelling the product identifier.
  \endlist

  \qml
  Store {
      Product {
          identifier: "myConsumableProduct"
          type: Product.Consumable
          onStatusChanged: {
              switch (status) {
              case Product.PendingRegistration: console.debug("Registering " + identifier); break
              case Product.Registered: console.debug(identifier + " registered with price " + price); break
              case Product.Unknown: console.debug(identifier + " was not found in the market place"); break
              }
          }

          // ...
      }
  }
  \endqml
*/
QInAppProductQmlType::Status QInAppProductQmlType::status() const
{
    return m_status;
}

/*!
  \qmlproperty string QtPurchasing::Product::price
  This property holds the price of the product once it has been successfully queried from the
  external market place. The price is a string consisting of both currency and value, and is
  usually localized to the current user.

  For example, the following example displays the price of the unlockable product named
  "myUnlockableProduct":
  \code
  Store {
      Product {
          id: myUnlockableProduct
          identifier: "myUnlockableProduct"
          type: Product.Unlockable

          // ...
      }
  }

  Text {
      text: myUnlockableProduct.status === Product.Registered
            ? "Price is " + myUnlockableProduct.price
            : "Price unknown at the moment"
  }
  \endcode

  When run in a Norwegian locale, this code could for instance display "Price is kr 6,00" for a one-dollar product.
*/
QString QInAppProductQmlType::price() const
{
    return m_product != 0 ? m_product->price() : QString();
}

/*!
  \qmlproperty string QtPurchasing::Product::title
  This property holds the title of the product once it has been successfully queried from the
  external market place. The title is localized if the external market place has defined a title
  in the current users locale.
*/
QString QInAppProductQmlType::title() const
{
    return m_product != 0 ? m_product->title() : QString();
}

/*!
  \qmlproperty string QtPurchasing::Product::description
  This property holds the description of the product once it has been successfully queried from the
  external market place. The title is localized if the external market place has defined a description
  in the current users locale.
*/
QString QInAppProductQmlType::description() const
{
    return m_product != 0 ? m_product->description() : QString();
}

void QInAppProductQmlType::setProduct(QInAppProduct *product)
{
    if (m_product == product)
        return;

    QString oldPrice = price();
    QString oldTitle = title();
    QString oldDescription = description();
    m_product = product;
    if (price() != oldPrice)
        emit priceChanged();
    if (title() != oldTitle)
        emit titleChanged();
    if (description() != oldDescription)
        emit descriptionChanged();
}

void QInAppProductQmlType::handleProductRegistered(QInAppProduct *product)
{
    if (product->identifier() == m_identifier) {
        Q_ASSERT(product->productType() == QInAppProduct::ProductType(m_type));
        setProduct(product);
        if (m_status != Registered) {
            m_status = Registered;
            emit statusChanged();
        }
    }
}

void QInAppProductQmlType::handleProductUnknown(QInAppProduct::ProductType, const QString &identifier)
{
    if (identifier == m_identifier) {
        setProduct(0);
        if (m_status != Unknown) {
            m_status = Unknown;
            emit statusChanged();
        }
    }
}

void QInAppProductQmlType::handleTransaction(QInAppTransaction *transaction)
{
    if (transaction->product()->identifier() != m_identifier)
        return;

    if (transaction->status() == QInAppTransaction::PurchaseApproved)
        emit purchaseSucceeded(transaction);
    else if (transaction->status() == QInAppTransaction::PurchaseRestored)
        emit purchaseRestored(transaction);
    else
        emit purchaseFailed(transaction);
}

/*!
  \qmlmethod QtPurchasing::Product::purchase()

  Launches the purchasing process for this product. The purchasing process is asynchronous.
  When it completes, either the \l onPurchaseSucceeded or the \l onPurchaseFailed handler
  in the object will be called with the resulting transaction.
*/
void QInAppProductQmlType::purchase()
{
    if (m_product != 0 && m_status == Registered)
        m_product->purchase();
    else
        qWarning("Attempted to purchase unregistered product");
}

/*!
  \qmlsignal QtPurchasing::Product::onPurchaseSucceeded(object transaction)

  This handler is called when a product has been purchased successfully. It is triggered
  when the application has called purchase() on the product and the user has subsequently
  confirmed the purchase, for example by entering their password.

  All products should have a handler for onPurchaseSucceeded. This handler should in turn
  save information about the purchased product and when the information has been stored
  and verified, it should call finalize() on the \a transaction object.

  The handler should support being called multiple times for the same purchase. For example,
  the application execution might by accident be interrupted after saving the purchase
  information, but before finalizing the transaction. In this case, the handler should
  verify that the information is already stored in the persistent storage and then finalize
  the transaction.

  The following example attempts to store the purchase state of a consumable
  product using a custom made function. It only finalizes the transaction if saving the
  data was successful. Otherwise, it calls another custom function to display an error
  message to the user.

  \qml
  Store {
      Product {
          id: myConsumableProduct
          identifier: "myConsumableProduct"
          type: Product.Consumable

          onPurchaseSucceeded: {
              if (myStorage.savePurchaseInformation(identifier)) {
                  transaction.finalize()
              } else {
                  myDisplayHelper.message("Failed to store purchase information. Is there available storage?")
              }
          }

          // ...
      }
  }
  \endqml

  If the transaction is not finalized, the onPurchaseSucceeded handler will be called again
  the next time the product is registered (on application startup.) This means that if saving
  the information failed, the user will have the opportunity of rectifying the problem (for
  example by deleting something else to make space for the data) and the transaction will
  be completed once they restart the application and the problem has been solved.

  \note A purchased, consumable product can not be purchased again until its previous transaction
  is finalized.
*/

/*!
  \qmlsignal QtPurchasing::Product::onPurchaseFailed(object transaction)

  This handler is called when a purchase was requested for a given product, but the purchase
  failed. This will typically happen if the application calls purchase() on a product, and
  the user subsequently cancels the purchase. It could also happen under other circumstances,
  for example if there is no suitable network connection.

  All products should have an \c onPurchaseFailed handler.

  After a proper reaction is taken, the finalize() function should be called on the \a transaction
  object. If this is not done, the handler may be called again the next time the product is registered.

  The following example reacts to a failed purchase attempt by calling a custom function to display a
  message to the user.
  \qml
  Store {
      Product {
          id: myConsumableProduct
          identifier: "myConsumableProduct"
          type: Product.Consumable

          onPurchaseFailed: {
              myDisplayHelper.message("Product was not purchased. You have not been charged.")
              transaction.finalize()
          }

          // ...
      }
  }
  \endqml
*/

/*!
  \qmlsignal QtPurchasing::Product::onPurchaseRestored(object transaction)

  This handler is called when a previously purchased unlockable product is restored. This
  can happen when the \l{QtPurchasing::Store::restorePurchases()}{restorePurchases()} function
  in the current \l Store is called.
  The \c onPurchaseRestored handler will then be called for each unlockable product which
  has previously been purchased by the user.

  Applications which uses the \l{QtPurchasing::Store::restorePurchases()}{restorePurchases()}
  function should include this handler
  in all unlockable products. In the handler, the application should make sure information
  about the purchase is stored and call \l{QtPurchasing::Transaction::finalize()}{finalize()}
  on the \a transaction object if
  the information has been successfully stored (or has been verified to already be stored).

  The following example calls a custom function which either saves the information about
  the purchase or verifies that it is already saved. When the data has been verified, it
  finalizes the transaction. If it could not be verified, it calls another custom function
  to display an error message to the user. If the transaction is not finalized, the handler
  will be called again for the same transaction the next time the product is registered
  (on application start-up).

  \qml
  Store {
      Product {
          id: myUnlockableProduct
          identifier: "myUnlockableProduct"
          type: Product.Unlockable

          onPurchaseRestored: {
              if (myStorage.savePurchaseInformation(identifier)) {
                  transaction.finalize()
              } else {
                  myDisplayHelper.message("Failed to store purchase information. Is there available storage?")
              }
          }

          // ...
      }
  }
  \endqml
*/

QT_END_NAMESPACE
