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

#ifndef QINAPPPRODUCTQMLTYPE_P_H
#define QINAPPPRODUCTQMLTYPE_P_H

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

#include <QtPurchasing/qinappproduct.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QInAppTransaction;
class QInAppStoreQmlType;
class QInAppProductQmlType : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_ENUMS(Status ProductType)
    Q_PROPERTY(QString identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(ProductType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QString price READ price NOTIFY priceChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QInAppStoreQmlType *store READ store WRITE setStore NOTIFY storeChanged)
public:
    enum Status {
        Uninitialized,
        PendingRegistration,
        Registered,
        Unknown
    };

    // Must match QInAppProduct::ProductType
    enum ProductType {
        Consumable,
        Unlockable
    };

    explicit QInAppProductQmlType(QObject *parent = 0);

    Q_INVOKABLE void purchase();
    Q_INVOKABLE void resetStatus();

    void setIdentifier(const QString &identifier);
    QString identifier() const;

    Status status() const;
    QString price() const;
    QString title() const;
    QString description() const;

    void setStore(QInAppStoreQmlType *store);
    QInAppStoreQmlType *store() const;

    void setType(ProductType type);
    ProductType type() const;

Q_SIGNALS:
    void purchaseSucceeded(QInAppTransaction *transaction);
    void purchaseFailed(QInAppTransaction *transaction);
    void purchaseRestored(QInAppTransaction *transaction);
    void identifierChanged();
    void statusChanged();
    void priceChanged();
    void titleChanged();
    void descriptionChanged();
    void storeChanged();
    void typeChanged();

protected:
    void componentComplete();
    void classBegin() {}

private Q_SLOTS:
    void handleTransaction(QInAppTransaction *transaction);
    void handleProductRegistered(QInAppProduct *product);
    void handleProductUnknown(QInAppProduct::ProductType, const QString &identifier);

private:
    void setProduct(QInAppProduct *product);
    void updateProduct();

    QString m_identifier;
    Status m_status;
    QInAppProductQmlType::ProductType m_type;
    bool m_componentComplete;

    QInAppStoreQmlType *m_store;
    QInAppProduct *m_product;
};

QT_END_NAMESPACE

#endif // QINAPPPRODUCTQMLTYPE_P_H
