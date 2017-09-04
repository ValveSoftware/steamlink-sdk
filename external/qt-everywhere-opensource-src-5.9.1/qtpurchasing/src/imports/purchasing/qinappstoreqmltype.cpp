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

#include "qinappstoreqmltype_p.h"
#include <QtPurchasing/qinappstore.h>

QT_BEGIN_NAMESPACE

/*!
  \qmltype Store
  \inqmlmodule QtPurchasing
  \since QtPurchasing 1.0
  \ingroup qtpurchasing
  \brief Access point to the external market place for in-app purchases.

  When using the Qt Purchasing API in QML, the application should instantiate
  one Store and then instantiate products as children of this store. The products
  created as children of the Store object will automatically be queried from the
  external market place if one is available on the current platform.

  The following example registers a store with three products, two consumable
  products and one unlockable.
  \qml
  Store {
      Product {
          identifier: "myConsumableProduct1"
          type: Product.Consumable

          // ...
      }

      Product {
          identifier: "myConsumableProduct2"
          type: Product.Consumable

          // ...
      }

      Product {
          identifier: "myUnlockableProduct"
          type: Product.Unlockable

          // ...
      }

      // ...
  }
  \endqml
 */

static void addProduct(QQmlListProperty<QInAppProductQmlType> *property, QInAppProductQmlType *product)
{
    QInAppStoreQmlType *store = qobject_cast<QInAppStoreQmlType *>(property->object);
    Q_ASSERT(store != 0);
    product->setStore(store);

    QList<QInAppProductQmlType *> *products = reinterpret_cast<QList<QInAppProductQmlType *> *>(property->data);
    Q_ASSERT(products != 0);

    products->append(product);
}

static int productCount(QQmlListProperty<QInAppProductQmlType> *property)
{
    QList<QInAppProductQmlType *> *products = reinterpret_cast<QList<QInAppProductQmlType *> *>(property->data);
    Q_ASSERT(products != 0);

    return products->size();
}

static void clearProducts(QQmlListProperty<QInAppProductQmlType> *property)
{
    QList<QInAppProductQmlType *> *products = reinterpret_cast<QList<QInAppProductQmlType *> *>(property->data);
    Q_ASSERT(products != 0);

    for (int i=0; i<products->size(); ++i) {
        QInAppProductQmlType *product = products->at(i);
        product->setStore(0);
    }
    products->clear();
}

static QInAppProductQmlType *productAt(QQmlListProperty<QInAppProductQmlType> *property, int index)
{
    QList<QInAppProductQmlType *> *m_products = reinterpret_cast<QList<QInAppProductQmlType *> *>(property->data);
    Q_ASSERT(m_products != 0);

    return m_products->at(index);
}

QInAppStoreQmlType::QInAppStoreQmlType(QObject *parent)
    : QObject(parent)
    , m_store(new QInAppStore(this))
{
}

QInAppStore *QInAppStoreQmlType::store() const
{
    return m_store;
}

QQmlListProperty<QInAppProductQmlType> QInAppStoreQmlType::products()
{
    return QQmlListProperty<QInAppProductQmlType>(this, &m_products, &addProduct, &productCount, &productAt, &clearProducts);
}

/*!
  \qmlmethod QtPurchasing::Store::restorePurchases()

  Calling this method will cause onPurchaseRestored handlers to be called
  asynchronously on previously purchased unlockable products. This can be
  used to restore purchases for unlockable products when the application is
  run by the same user on multiple devices, or for example if the application
  has been uninstalled and reinstalled on the device so that the purchase data
  has been lost.

  \note On some platforms, such as iOS, this will require the user to input their
  password to launch the restore process. On other platforms, such as Android,
  it is not typically needed, as the onPurchaseSucceeded handler will be called
  on any previously purchased unlockable product if the application data is
  removed.
 */
void QInAppStoreQmlType::restorePurchases()
{
    m_store->restorePurchases();
}

QT_END_NAMESPACE
