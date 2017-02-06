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

#include "qandroidinapppurchasebackend_p.h"
#include "qandroidinappproduct_p.h"
#include "qandroidinapptransaction_p.h"
#include "qinappstore.h"

#include <QtAndroidExtras/qandroidfunctions.h>
#include <QtAndroidExtras/qandroidjnienvironment.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qstandardpaths.h>

QT_BEGIN_NAMESPACE

// #define QANDROIDINAPPPURCHASEBACKEND_DEBUG

QAndroidInAppPurchaseBackend::QAndroidInAppPurchaseBackend(QObject *parent)
    : QInAppPurchaseBackend(parent)
    , m_mutex(QMutex::Recursive)
    , m_isReady(false)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Creating backend");
#endif

    m_javaObject = QAndroidJniObject("org/qtproject/qt5/android/purchasing/QtInAppPurchase",
                                       "(Landroid/content/Context;J)V",
                                       QtAndroid::androidActivity().object<jobject>(),
                                       reinterpret_cast<jlong>(this));
    if (!m_javaObject.isValid()) {
        qWarning("Cannot initialize IAP backend for Android due to missing dependency: QtInAppPurchase class");
        return;
    }
}

QString QAndroidInAppPurchaseBackend::finalizedUnlockableFileName() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return path + QStringLiteral("/.qt-purchasing-data/iap_finalization.data");
}

void QAndroidInAppPurchaseBackend::initialize()
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Initializing backend");
#endif

    m_javaObject.callMethod<void>("initializeConnection");

    QFile file(finalizedUnlockableFileName());
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);
        while (!stream.atEnd()) {
            QString identifier;
            stream >> identifier;
            m_finalizedUnlockableProducts.insert(identifier);

#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
            qDebug("Finalized unlockable: %s", qPrintable(identifier));
#endif

        }

    } else if (file.exists()) {
        qWarning("Failed to read from finalization data.");
    }
}

bool QAndroidInAppPurchaseBackend::isReady() const
{
    QMutexLocker locker(&m_mutex);
    return m_isReady;
}

void QAndroidInAppPurchaseBackend::restorePurchases()
{
    QSet<QString> previouslyFinalizedUnlockables = m_finalizedUnlockableProducts;
    m_finalizedUnlockableProducts.clear();
    foreach (QString previouslyFinalizedUnlockable, previouslyFinalizedUnlockables) {
        QInAppProduct *product = store()->registeredProduct(previouslyFinalizedUnlockable);
        Q_ASSERT(product != 0);

        checkFinalizationStatus(product, QInAppTransaction::PurchaseRestored);
    }
}

void QAndroidInAppPurchaseBackend::queryProducts(const QList<Product> &products)
{
    QMutexLocker locker(&m_mutex);
    QAndroidJniEnvironment environment;

    QStringList newProducts;
    for (int i = 0; i < products.size(); ++i) {
        const Product &product = products.at(i);
        if (m_productTypeForPendingId.contains(product.identifier)) {
            qWarning("Product query already pending for %s", qPrintable(product.identifier));
            continue;
        }

        m_productTypeForPendingId[product.identifier] = product.productType;
        newProducts.append(product.identifier);
    }

    if (newProducts.isEmpty())
        return;

    jclass cls = environment->FindClass("java/lang/String");
    jobjectArray productIds = environment->NewObjectArray(newProducts.size(), cls, 0);
    environment->DeleteLocalRef(cls);

    for (int i = 0; i < newProducts.size(); ++i) {
        QAndroidJniObject identifier = QAndroidJniObject::fromString(newProducts.at(i));
        environment->SetObjectArrayElement(productIds, i, identifier.object());
    }

    m_javaObject.callMethod<void>("queryDetails",
                                  "([Ljava/lang/String;)V",
                                  productIds);
    environment->DeleteLocalRef(productIds);
}

void QAndroidInAppPurchaseBackend::queryProduct(QInAppProduct::ProductType productType,
                                                const QString &identifier)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Querying product: %s (%d)", qPrintable(identifier), productType);
#endif

    queryProducts(QList<Product>() << Product(productType, identifier));
}

void QAndroidInAppPurchaseBackend::setPlatformProperty(const QString &propertyName, const QString &value)
{
    QMutexLocker locker(&m_mutex);
    if (propertyName.compare(QStringLiteral("AndroidPublicKey"), Qt::CaseInsensitive) == 0) {
        m_javaObject.callMethod<void>("setPublicKey",
                                      "(Ljava/lang/String;)V",
                                      QAndroidJniObject::fromString(value).object<jstring>());
    }
}

void QAndroidInAppPurchaseBackend::registerQueryFailure(const QString &productId)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Query failed for %s", qPrintable(productId));
#endif

    QMutexLocker locker(&m_mutex);
    QHash<QString, QInAppProduct::ProductType>::iterator it = m_productTypeForPendingId.find(productId);
    Q_ASSERT(it != m_productTypeForPendingId.end());

    QInAppProduct::ProductType productType = it.value();
    m_productTypeForPendingId.erase(it);
    emit productQueryFailed(productType, productId);
}

void QAndroidInAppPurchaseBackend::consumeTransaction(const QString &purchaseToken)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Transaction consumed for %s", qPrintable(purchaseToken));
#endif

    QMutexLocker locker(&m_mutex);
    m_javaObject.callMethod<void>("consumePurchase",
                                  "(Ljava/lang/String;)V",
                                  QAndroidJniObject::fromString(purchaseToken).object<jstring>());
}

void QAndroidInAppPurchaseBackend::registerFinalizedUnlockable(const QString &identifier)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Finalizing unlockable %s", qPrintable(identifier));
#endif

    QMutexLocker locker(&m_mutex);
    m_finalizedUnlockableProducts.insert(identifier);

    QString fileName = finalizedUnlockableFileName();
    QDir().mkpath(QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Failed to open file to store finalization info.");
        return;
    }

    QDataStream stream(&file);
    foreach (QString finalizedUnlockableProduct, m_finalizedUnlockableProducts)
        stream << finalizedUnlockableProduct;
}

bool QAndroidInAppPurchaseBackend::transactionFinalizedForProduct(QInAppProduct *product)
{
    Q_ASSERT(m_infoForPurchase.contains(product->identifier()));
    return product->productType() != QInAppProduct::Consumable
            && m_finalizedUnlockableProducts.contains(product->identifier());
}

void QAndroidInAppPurchaseBackend::checkFinalizationStatus(QInAppProduct *product,
                                                           QInAppTransaction::TransactionStatus status)
{
    // Verifies the finalization status of an item based on the following logic:
    // 1. If the item is not purchased yet, do nothing (it's either never been purchased, or it's a
    //    consumed consumable.
    // 2. If the item is purchased, and it's a consumable, it's unfinalized. Emit a new transaction.
    //    Consumable items are consumed when they are finalized.
    // 3. If the item is purchased, and it's an unlockable, check the local cache for finalized
    //    unlockable purchases. If it's not there, then the transaction is unfinalized. This means
    //    that if the cache gets deleted or corrupted, the worst-case scenario is that the transactions
    //    are republished.
    QHash<QString, PurchaseInfo>::iterator it = m_infoForPurchase.find(product->identifier());
    if (it == m_infoForPurchase.end()) {
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
        qDebug("Product %s not purchased", qPrintable(product->identifier()));
#endif
        return;
    }

    const PurchaseInfo &info = it.value();
    if (!transactionFinalizedForProduct(product)) {
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
        qDebug("Product unfinalized: %s. Emitting transaction with status %d.", qPrintable(product->identifier()), status);
#endif

        QAndroidInAppTransaction *transaction = new QAndroidInAppTransaction(info.signature,
                                                                             info.data,
                                                                             info.purchaseToken,
                                                                             info.orderId,
                                                                             status,
                                                                             product,
                                                                             info.timestamp,
                                                                             QInAppTransaction::NoFailure,
                                                                             QString(),
                                                                             this);
        emit transactionReady(transaction);
    }
}

void QAndroidInAppPurchaseBackend::registerProduct(const QString &productId,
                                                   const QString &price,
                                                   const QString &title,
                                                   const QString &description)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Registering product %s with price %s", qPrintable(productId), qPrintable(price));
#endif

    QMutexLocker locker(&m_mutex);
    QHash<QString, QInAppProduct::ProductType>::iterator it = m_productTypeForPendingId.find(productId);
    Q_ASSERT(it != m_productTypeForPendingId.end());

    QAndroidInAppProduct *product = new QAndroidInAppProduct(this, price, title, description, it.value(), it.key(), this);
    checkFinalizationStatus(product);

    emit productQueryDone(product);
    m_productTypeForPendingId.erase(it);
}

void QAndroidInAppPurchaseBackend::registerPurchased(const QString &identifier,
                                                     const QString &signature,
                                                     const QString &data,
                                                     const QString &purchaseToken,
                                                     const QString &orderId,
                                                     const QDateTime &timestamp)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Registering previously purchased product: %s", qPrintable(identifier));
#endif

    QMutexLocker locker(&m_mutex);
    m_infoForPurchase.insert(identifier, PurchaseInfo(signature, data, purchaseToken, orderId, timestamp));
}

void QAndroidInAppPurchaseBackend::registerReady()
{
    QMutexLocker locker(&m_mutex);
    m_isReady = true;
    emit ready();
}

void QAndroidInAppPurchaseBackend::handleActivityResult(int requestCode, int resultCode, const QAndroidJniObject &data)
{
    QInAppProduct *product = m_activePurchaseRequests.value(requestCode);
    if (product == 0) {
        qWarning("No product registered for requestCode %d", requestCode);
        return;
    }

    m_javaObject.callMethod<void>("handleActivityResult", "(IILandroid/content/Intent;Ljava/lang/String;)V",
                                  requestCode,
                                  resultCode,
                                  data.object<jobject>(),
                                  QAndroidJniObject::fromString(product->identifier()).object<jstring>());
}

void QAndroidInAppPurchaseBackend::purchaseProduct(QAndroidInAppProduct *product)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Attempting to purchase %s", qPrintable(product->identifier()));
#endif

    QMutexLocker locker(&m_mutex);
    if (!m_javaObject.isValid()) {
        purchaseFailed(product, QInAppTransaction::ErrorOccurred, QStringLiteral("Java backend is not initialized"));
        return;
    }

    QAndroidJniObject intentSender = m_javaObject.callObjectMethod("createBuyIntentSender",
                                                                   "(Ljava/lang/String;)Landroid/content/IntentSender;",
                                                                   QAndroidJniObject::fromString(product->identifier()).object<jstring>());
    if (!intentSender.isValid()) {
        purchaseFailed(product, QInAppTransaction::ErrorOccurred, QStringLiteral("Unable to get intent sender from service"));
        return;
    }

    int requestCode = 0;
    while (m_activePurchaseRequests.contains(requestCode)) {
        requestCode++;
        if (requestCode == 0) {
            qWarning("No available request code for purchase request.");
            return;
        }
    }

    m_activePurchaseRequests[requestCode] = product;
    QtAndroid::startIntentSender(intentSender, requestCode, this);
}

void QAndroidInAppPurchaseBackend::purchaseFailed(int requestCode, int failureReason, const QString &errorString)
{
    QMutexLocker locker(&m_mutex);
    QInAppProduct *product = m_activePurchaseRequests.take(requestCode);
    if (product == 0) {
        qWarning("No product registered for requestCode %d", requestCode);
        return;
    }

    purchaseFailed(product, failureReason, errorString);
}

void QAndroidInAppPurchaseBackend::purchaseFailed(QInAppProduct *product, int failureReason, const QString &errorString)
{
#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Purchase failed for %s", qPrintable(product->identifier()));
#endif

    QInAppTransaction *transaction = new QAndroidInAppTransaction(QString(),
                                                                  QString(),
                                                                  QString(),
                                                                  QString(),
                                                                  QInAppTransaction::PurchaseFailed,
                                                                  product,
                                                                  QDateTime(),
                                                                  QInAppTransaction::FailureReason(failureReason),
                                                                  errorString,
                                                                  this);
    emit transactionReady(transaction);
}

void QAndroidInAppPurchaseBackend::purchaseSucceeded(int requestCode,
                                                     const QString &signature,
                                                     const QString &data,
                                                     const QString &purchaseToken,
                                                     const QString &orderId,
                                                     const QDateTime &timestamp)

{
    QMutexLocker locker(&m_mutex);
    QInAppProduct *product = m_activePurchaseRequests.take(requestCode);
    if (product == 0) {
        qWarning("No product registered for requestCode %d", requestCode);
        return;
    }

#if defined(QANDROIDINAPPPURCHASEBACKEND_DEBUG)
    qDebug("Purchase succeeded for %s", qPrintable(product->identifier()));
#endif


    m_infoForPurchase.insert(product->identifier(), PurchaseInfo(signature, data, purchaseToken, orderId, timestamp));
    QInAppTransaction *transaction = new QAndroidInAppTransaction(signature,
                                                                  data,
                                                                  purchaseToken,
                                                                  orderId,
                                                                  QInAppTransaction::PurchaseApproved,
                                                                  product,
                                                                  timestamp,
                                                                  QInAppTransaction::NoFailure,
                                                                  QString(),
                                                                  this);
    emit transactionReady(transaction);
}

QT_END_NAMESPACE
