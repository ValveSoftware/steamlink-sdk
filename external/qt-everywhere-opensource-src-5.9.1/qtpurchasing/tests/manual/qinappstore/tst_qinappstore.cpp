/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtPurchasing/QInAppStore>
#include <QtPurchasing/QInAppProduct>

class tst_QInAppStore: public QObject
{
    Q_OBJECT
private slots:
    void registerProduct();
};

class SignalReceiver: public QObject
{
    Q_OBJECT
public:
    QList<QInAppTransaction *> readyTransactions;
    QList<QInAppProduct *> registeredProducts;
    QList<QPair<QInAppProduct::ProductType, QString> > unknownProducts;

public slots:
    void productRegistered(QInAppProduct *product)
    {
        registeredProducts.append(product);
    }

    void productUnknown(QInAppProduct::ProductType productType, const QString &identifier)
    {
        unknownProducts.append(qMakePair(productType, identifier));
    }

    void transactionReady(QInAppTransaction *transaction)
    {
        readyTransactions.append(transaction);
    }
};

void tst_QInAppStore::registerProduct()
{
    QInAppStore store(this);
    SignalReceiver receiver;

    connect(&store, SIGNAL(productRegistered(QInAppProduct*)), &receiver, SLOT(productRegistered(QInAppProduct*)));
    connect(&store, SIGNAL(productUnknown(QInAppProduct::ProductType,QString)), &receiver, SLOT(productUnknown(QInAppProduct::ProductType,QString)));
    connect(&store, SIGNAL(transactionReady(QInAppTransaction*)), &receiver, SLOT(transactionReady(QInAppTransaction*)));

    store.registerProduct(QInAppProduct::Consumable, QStringLiteral("consumable"));
    store.registerProduct(QInAppProduct::Unlockable, QStringLiteral("unlockable"));

    QTRY_COMPARE(receiver.registeredProducts.size(), 2);
    QCOMPARE(receiver.unknownProducts.size(), 0);
    QCOMPARE(receiver.readyTransactions.size(), 0);

    int found = 0;
    for (int i = 0; i < receiver.registeredProducts.size(); ++i) {
        QInAppProduct *product = receiver.registeredProducts.at(i);
        if ((product->identifier() == QStringLiteral("consumable") && product->productType() == QInAppProduct::Consumable)
                || (product->identifier() == QStringLiteral("unlockable") && product->productType() == QInAppProduct::Unlockable)) {
            found++;
        }

        switch (product->productType()) {
        case QInAppProduct::Consumable:
            QCOMPARE(product->description(), QStringLiteral("consumableDescription"));
            QCOMPARE(product->title(), QStringLiteral("consumableTitle"));
            break;
        case QInAppProduct::Unlockable:
            QCOMPARE(product->description(), QStringLiteral("unlockableDescription"));
            QCOMPARE(product->title(), QStringLiteral("unlockableTitle"));
            break;
        }

        QInAppProduct *registeredProduct = store.registeredProduct(product->identifier());
        QCOMPARE(registeredProduct->identifier(), product->identifier());
        QCOMPARE(registeredProduct->productType(), product->productType());
        QCOMPARE(registeredProduct->title(), product->title());
        QCOMPARE(registeredProduct->description(), product->description());
        QCOMPARE(registeredProduct->price(), product->price());
    }

    QCOMPARE(found, 2);
}

QTEST_MAIN(tst_QInAppStore)

#include "tst_qinappstore.moc"

