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

#include "qinappproduct.h"

QT_BEGIN_NAMESPACE

class QInAppProductPrivate
{
public:
    QInAppProductPrivate(const QString &price, const QString &title, const QString &description, QInAppProduct::ProductType type, const QString &id)
        : localPrice(price)
        , localTitle(title)
        , localDescription(description)
        , productType(type)
        , identifier(id)
    {
    }

    QString localPrice;
    QString localTitle;
    QString localDescription;
    QInAppProduct::ProductType productType;
    QString identifier;
};

/*!
   \class QInAppProduct
   \inmodule QtPurchasing
   \brief A product registered in the store

   QInAppProduct encapsulates a product in the external store after it has been registered in \c QInAppStore
   and confirmed to exist. It has an identifier which matches the identifier of the product in the external
   store, it has a price which is retrieved from the external store, and it has a product type.

   The product type can be either \c Consumable or \c Unlockable. The former type of products can be purchased
   any number of times as long as each transaction is finalized explicitly by the application. The latter type
   can only be purchased once.
 */

/*!
 * \internal
 */\
QInAppProduct::QInAppProduct(const QString &price, const QString &title, const QString &description, ProductType productType, const QString &identifier, QObject *parent)
    : QObject(parent)
{
    d = QSharedPointer<QInAppProductPrivate>(new QInAppProductPrivate(price, title, description, productType, identifier));
}

/*!
 * \internal
 */\
QInAppProduct::~QInAppProduct()
{
}

/*!
  \property QInAppProduct::price

  This property holds the price of the product as reported by the external store. This is the full
  price including currency, usually in the locale of the current user.
*/
QString QInAppProduct::price() const
{
    return d->localPrice;
}

/*!
 * \property QInAppProduct::title
 *
 * This property holds the title of the product as reported by the external store.  This title is returned from the
 * store in the locale language if available.
 */
QString QInAppProduct::title() const
{
    return d->localTitle;
}

/*!
 * \property QInAppProduct::description
 *
 * This property holds the description of the product as reported by the external store.  This description is returned
 * from the store in the locale language if available.
 */
QString QInAppProduct::description() const
{
    return d->localDescription;
}

/*!
  \property QInAppProduct::identifier

  This property holds the identifier of the product. It matches the identifier which is registered in
  the external store.
*/
QString QInAppProduct::identifier() const
{
    return d->identifier;
}

/*!
  \enum QInAppProduct::ProductType

  This enum type is used to specify the product type when registering the product.

  \value Consumable The product is consumable, meaning that once the transaction for a purchase of
  the product has been finalized, it can be purchased again.
  \value Unlockable The product is unlockable, meaning that it can only be purchased once per
  user. Purchases of unlockable products can be restored using the \l{QInAppStore::restorePurchases()}.
*/

/*!
 \property QInAppProduct::productType

 This property holds the type of the product. This can either be \c Consumable or \c Unlockable. The
 former are products which can be purchased any number of times (granted that each transaction is
 explicitly finalized by the application first) and the latter are products which can only be purchased
 once per user.
 */
QInAppProduct::ProductType QInAppProduct::productType() const
{
    return d->productType;
}

/*!
 * \fn void QInAppProduct::purchase()
 *
 * Launches the purchase flow for this product. The purchase is done asynchronously. When the purchase has
 * either been completed successfully or failed for some reason, the QInAppStore instance containing
 * this product will emit a QInAppStore::transactionReady() signal with information about the transaction.
 *
 * \sa QInAppTransaction
 */

QT_END_NAMESPACE
