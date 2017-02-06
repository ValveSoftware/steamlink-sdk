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

#include <QtTest/QtTest>
#include <QtPurchasing/QInAppStore>

class tst_QInAppStore: public QObject
{
    Q_OBJECT
private slots:
    void registerUnknownProduct();
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

void tst_QInAppStore::registerUnknownProduct()
{
    QInAppStore store(this);
    SignalReceiver receiver;

    connect(&store, SIGNAL(productRegistered(QInAppProduct*)), &receiver, SLOT(productRegistered(QInAppProduct*)));
    connect(&store, SIGNAL(productUnknown(QInAppProduct::ProductType,QString)), &receiver, SLOT(productUnknown(QInAppProduct::ProductType,QString)));
    connect(&store, SIGNAL(transactionReady(QInAppTransaction*)), &receiver, SLOT(transactionReady(QInAppTransaction*)));

    store.registerProduct(QInAppProduct::Consumable, QStringLiteral("unknownConsumable"));
    store.registerProduct(QInAppProduct::Unlockable, QStringLiteral("unknownUnlockable"));

//The backend is implemented on iOS, macOS, WinRT and Android, for others we expect failure.
#if !(defined(Q_OS_DARWIN) && !defined(Q_OS_WATCHOS)) && !defined(Q_OS_ANDROID) && !defined(Q_OS_WINRT)
    QEXPECT_FAIL("", "Qt Purchasing not implemented on this platform.", Abort);
#endif

    //Due to network overload or connectivity issues QTRY_COMPARE sometimes fails with timeout,
    //that's why we need to increase the value, since it's better to wait than to fail.
    QTRY_COMPARE_WITH_TIMEOUT(receiver.unknownProducts.size(), 2, 10000);
    QCOMPARE(receiver.registeredProducts.size(), 0);
    QCOMPARE(receiver.readyTransactions.size(), 0);

    int found = 0;
    for (int i = 0; i < receiver.unknownProducts.size(); ++i) {
        const QPair<QInAppProduct::ProductType, QString> &product = receiver.unknownProducts.at(i);

        if ((product.first == QInAppProduct::Unlockable && product.second == QStringLiteral("unknownUnlockable"))
            || (product.first == QInAppProduct::Consumable && product.second == QStringLiteral("unknownConsumable"))) {
            found++;
        }
    }

    QCOMPARE(found, 2);
}

QTEST_MAIN(tst_QInAppStore)

#include "tst_qinappstore.moc"
