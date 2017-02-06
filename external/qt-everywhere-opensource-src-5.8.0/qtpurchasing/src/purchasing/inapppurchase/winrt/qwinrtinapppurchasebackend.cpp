/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qwinrtinapppurchasebackend_p.h"
#include "qwinrtinappproduct_p.h"
#include "qwinrtinapptransaction_p.h"
#include "qinappstore.h"

#include <QLoggingCategory>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QXmlStreamReader>

#include <private/qeventdispatcher_winrt_p.h>
#include <qfunctions_winrt.h>
#include <functional>

#include <Windows.ApplicationModel.store.h>
#include <Windows.Applicationmodel.Activation.h>
#include <wrl.h>

using namespace ABI::Windows::ApplicationModel::Store;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Storage;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

typedef IAsyncOperationCompletedHandler<ListingInformation *> ListingInformationHandler;

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcPurchasingBackend, "qt.purchasing.backend")

const QString qt_win_app_identifier = QLatin1String("app");

inline QString hStringToQString(const HString &h)
{
    unsigned int length;
    const wchar_t* raw = h.GetRawBuffer(&length);
    return QString::fromWCharArray(raw, length);
}

class QWinRTAppBridge {
public:
    HRESULT activate();
    HRESULT LoadListingInformationAsync(ComPtr<IAsyncOperation<ListingInformation *>> &op);
    HRESULT GetAppReceiptAsync(ComPtr<IAsyncOperation<HSTRING>> &op);
    HRESULT RequestAppPurchaseAsync(bool receipt, ComPtr<IAsyncOperation<HSTRING>> &op);
    HRESULT RequestProductPurchaseAsync(HSTRING productId, bool receipt, ComPtr<IAsyncOperation<HSTRING>> &op);
    HRESULT RequestProductPurchaseWithResultsAsync(HSTRING productId, ComPtr<IAsyncOperation<PurchaseResults *>> &purchaseOp);
    HRESULT ReportConsumableFulfillmentAsync(HSTRING productId, GUID transactionId, ComPtr<IAsyncOperation<FulfillmentResult>> &op);
    HRESULT get_LicenseInformation(ComPtr<ILicenseInformation> &licenseInfo);
private:
    HRESULT qt_winrt_load_simulator_config(const QString &fileName, ComPtr<ICurrentAppSimulator> &simulator);
    ComPtr<ICurrentAppSimulator> m_simulator;
    ComPtr<ICurrentApp> m_app;
    bool m_simulate{false};
};

HRESULT QWinRTAppBridge::activate()
{
    HRESULT hr;
    const QString storeFilename = QLatin1String("QtStoreSimulation.xml");
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/');
    const QString storeSourceName = QLatin1String(":/") + storeFilename;
    const QString storeTargetName = dataPath + storeFilename;
    if (QFileInfo::exists(storeSourceName)) {
        qWarning("Found purchasing simulation file, disabling store connectivity.\n"
                 "Please note you will not be able to publish this package.");
        bool success = true;
        if (QFileInfo(storeTargetName).exists())
            success = QFile(storeTargetName).remove();

        if (!success)
            qWarning("Could not remove previous purchasing simulation file.");

        success = QFile(storeSourceName).copy(storeTargetName);
        if (!success)
            qWarning("Could not copy purchasing simulation file.");

        success = QFile::setPermissions(storeTargetName,
                                        QFile::WriteOwner | QFile::ReadOwner | QFile::WriteUser | QFile::ReadUser);
        if (!success)
            qWarning("Could not set readwrite flags for purchasing simulation file");

        m_simulate = true;
    }

    if (m_simulate) {
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_ApplicationModel_Store_CurrentAppSimulator).Get(),
                                    IID_PPV_ARGS(&m_simulator));
        qt_winrt_load_simulator_config(storeTargetName, m_simulator);
    } else {
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_ApplicationModel_Store_CurrentApp).Get(),
                                    IID_PPV_ARGS(&m_app));
    }
    return hr;
}

HRESULT QWinRTAppBridge::LoadListingInformationAsync(ComPtr<IAsyncOperation<ListingInformation *> > &op)
{
    HRESULT hr;
    if (m_simulate)
        hr = m_simulator->LoadListingInformationAsync(&op);
    else
        hr = m_app->LoadListingInformationAsync(&op);
    return hr;
}

HRESULT QWinRTAppBridge::GetAppReceiptAsync(ComPtr<IAsyncOperation<HSTRING> > &op)
{
    HRESULT hr;
    if (m_simulate)
        hr = m_simulator->GetAppReceiptAsync(&op);
    else
        hr = m_app->GetAppReceiptAsync(&op);
    return hr;
}

HRESULT QWinRTAppBridge::RequestAppPurchaseAsync(bool receipt, ComPtr<IAsyncOperation<HSTRING> > &op)
{
    HRESULT hr;
    if (m_simulate)
        hr = m_simulator->RequestAppPurchaseAsync(receipt, op.GetAddressOf());
    else
        hr = m_app->RequestAppPurchaseAsync(receipt, op.GetAddressOf());
    return hr;
}

HRESULT QWinRTAppBridge::RequestProductPurchaseAsync(HSTRING productId, bool receipt, ComPtr<IAsyncOperation<HSTRING> > &op)
{
    HRESULT hr;
    if (m_simulate)
        hr = m_simulator->RequestProductPurchaseAsync(productId, receipt, op.GetAddressOf());
    else
        hr = m_app->RequestProductPurchaseAsync(productId, receipt, op.GetAddressOf());
    return hr;
}

HRESULT QWinRTAppBridge::RequestProductPurchaseWithResultsAsync(HSTRING productId, ComPtr<IAsyncOperation<PurchaseResults *> > &purchaseOp)
{
    HRESULT hr;
    if (m_simulate) {
        ComPtr<ICurrentAppSimulatorWithConsumables> consumApp;
        hr = m_simulator.As(&consumApp);
        Q_ASSERT_SUCCEEDED(hr);
        hr = consumApp->RequestProductPurchaseWithResultsAsync(productId, purchaseOp.GetAddressOf());
    } else {
        ComPtr<ICurrentAppWithConsumables> consumApp;
        hr = m_app.As(&consumApp);
        Q_ASSERT_SUCCEEDED(hr);
        hr = consumApp->RequestProductPurchaseWithResultsAsync(productId, purchaseOp.GetAddressOf());
    }
    return hr;
}

HRESULT QWinRTAppBridge::ReportConsumableFulfillmentAsync(HSTRING productId, GUID transactionId, ComPtr<IAsyncOperation<FulfillmentResult> > &op)
{
    HRESULT hr;
    if (m_simulate) {
        ComPtr<ICurrentAppSimulatorWithConsumables> consumApp;
        hr = m_simulator.As(&consumApp);
        Q_ASSERT_SUCCEEDED(hr);
        hr = consumApp->ReportConsumableFulfillmentAsync(productId, transactionId, op.GetAddressOf());
    } else {
        ComPtr<ICurrentAppWithConsumables> consumApp;
        hr = m_app.As(&consumApp);
        Q_ASSERT_SUCCEEDED(hr);
        hr = consumApp->ReportConsumableFulfillmentAsync(productId, transactionId, op.GetAddressOf());
    }
    return hr;
}

HRESULT QWinRTAppBridge::get_LicenseInformation(ComPtr<ILicenseInformation> &licenseInfo)
{
    HRESULT hr;
    if (m_simulate) {
        hr = m_simulator->get_LicenseInformation(&licenseInfo);
    } else {
        hr = m_app->get_LicenseInformation(&licenseInfo);
    }
    return hr;
}

HRESULT QWinRTAppBridge::qt_winrt_load_simulator_config(const QString &fileName, ComPtr<ICurrentAppSimulator> &simulator)
{
    qCDebug(lcPurchasingBackend) << __FUNCTION__ << fileName;

    HRESULT hr;
    const QString nativeFilename = QDir::toNativeSeparators(fileName);
    HString nativeName;
    hr = nativeName.Set(reinterpret_cast<PCWSTR>(nativeFilename.utf16()), nativeFilename.size());
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IStorageFileStatics> fileStatics;
    hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Storage_StorageFile).Get(), &fileStatics);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IAsyncOperation<StorageFile*>> op;
    hr = fileStatics->GetFileFromPathAsync(nativeName.Get(), &op);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IStorageFile> storeFile;
    hr = QWinRTFunctions::await(op, storeFile.GetAddressOf());
    RETURN_HR_IF_FAILED("Could not find purchasing simulator xml description.");

    ComPtr<ABI::Windows::Foundation::IAsyncAction> reloadAction;
    hr = simulator->ReloadSimulatorAsync(storeFile.Get(), &reloadAction);
    Q_ASSERT_SUCCEEDED(hr);

    hr = QWinRTFunctions::await(reloadAction);
    RETURN_HR_IF_FAILED("Failed to load purchasing description.");

    return hr;
}

struct NativeProductInfo
{
    NativeProductInfo() { }
    HString productID;
    HString formatPrice;
    HString productName;
    ProductType type;
};

inline bool compareProductTypes(QInAppProduct::ProductType qtType, ProductType nativeType) {
    if (qtType == QInAppProduct::Consumable && nativeType == ProductType_Consumable)
        return true;
    else if (qtType == QInAppProduct::Unlockable && nativeType == ProductType_Durable)
        return true;
    else
        return false;
}

void QWinRTInAppPurchaseBackend::createTransactionDelayed(qt_WinRTTransactionData data)
{
    QInAppTransaction::TransactionStatus qStatus = (data.status == AsyncStatus::Completed) ?
                                                    QInAppTransaction::PurchaseApproved : QInAppTransaction::PurchaseFailed;

    QInAppTransaction::FailureReason reason;
    switch (data.status) {
    case AsyncStatus::Completed:
        reason = QInAppTransaction::NoFailure;
        break;
    case AsyncStatus::Canceled:
        reason = QInAppTransaction::CanceledByUser;
        break;
    case AsyncStatus::Error:
    default:
        reason = QInAppTransaction::ErrorOccurred;
        break;
    }

    auto transaction = new QWinRTInAppTransaction(qStatus, data.product, reason, data.receipt, this);
    transaction->m_purchaseResults = data.purchaseResults;
    emit transactionReady(transaction);

    return;
}

class QWinRTInAppPurchaseBackendPrivate
{
public:
    explicit QWinRTInAppPurchaseBackendPrivate(QWinRTInAppPurchaseBackend *p)
        : q_ptr(p)
    { }

    HRESULT onListingInformation(IAsyncOperation<ListingInformation*> *args,
                                                             AsyncStatus status);

    ComPtr<IProductLicense> findProductLicense(const QString &identifier);
    QWinRTAppBridge m_bridge;
    bool m_waitingForList = false;

    QMap<QString, NativeProductInfo*> nativeProducts;

    QWinRTInAppPurchaseBackend *q_ptr;
    Q_DECLARE_PUBLIC(QWinRTInAppPurchaseBackend)
};

QWinRTInAppPurchaseBackend::QWinRTInAppPurchaseBackend(QObject *parent)
    : QInAppPurchaseBackend(parent)
{
    d_ptr.reset(new QWinRTInAppPurchaseBackendPrivate(this));

    qRegisterMetaType<qt_WinRTTransactionData>("qt_WinRTTransactionData");

    qCDebug(lcPurchasingBackend) << __FUNCTION__;
}

void QWinRTInAppPurchaseBackend::initialize()
{
    Q_D(QWinRTInAppPurchaseBackend);
    qCDebug(lcPurchasingBackend) << __FUNCTION__;

    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HRESULT hr;
        hr = d->m_bridge.activate();
        Q_ASSERT_SUCCEEDED(hr);

        // ### Keep for later usage.
        // ComPtr<ILicenseInformation> licenseInfo;
        // hr = d->m_app->get_LicenseInformation(&licenseInfo);
        // RETURN_HR_IF_FAILED("Could not acquire license information.");

        ComPtr<IAsyncOperation<ListingInformation*>> op;
        hr = d->m_bridge.LoadListingInformationAsync(op);
        RETURN_HR_IF_FAILED("Purchasing: Could not load listing information.");

        hr = op->put_Completed(Callback<ListingInformationHandler>(d, &QWinRTInAppPurchaseBackendPrivate::onListingInformation).Get());
        Q_ASSERT_SUCCEEDED(hr);
        d->m_waitingForList = true;
        return S_OK;

    });
    RETURN_VOID_IF_FAILED("Could not initialize purchase backend");
}

bool QWinRTInAppPurchaseBackend::isReady() const
{
    Q_D(const QWinRTInAppPurchaseBackend);
    qCDebug(lcPurchasingBackend) << __FUNCTION__;
    return !d->m_waitingForList && !d->nativeProducts.isEmpty();
}

inline QString createStringForSubReceipt(const QXmlStreamReader &reader)
{
    QString result;
    QXmlStreamWriter writer(&result);
    writer.writeStartDocument();
    writer.writeCurrentToken(reader);
    writer.writeEndDocument();
    return result;
}

void QWinRTInAppPurchaseBackend::restorePurchases()
{
    qCDebug(lcPurchasingBackend) << __FUNCTION__;
    Q_D(QWinRTInAppPurchaseBackend);

    HRESULT hr;
    ComPtr<IAsyncOperation<HSTRING>> op;
    hr = d->m_bridge.GetAppReceiptAsync(op);

    if (FAILED(hr)) {
        qCDebug(lcPurchasingBackend) << "Failed to query receipts";
        return;
    }

    HString receipt;
    hr = QWinRTFunctions::await(op, receipt.GetAddressOf());

    if (FAILED(hr)) {
        qCDebug(lcPurchasingBackend) << "Could not wait for receipt";
        return;
    }

    const QString parse = hStringToQString(receipt);

    QXmlStreamReader reader(parse);
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Receipt"))
            break;
    }
    if (reader.name() != QLatin1String("Receipt")) {
        qCDebug(lcPurchasingBackend) << "Could not parse app receipt xml";
        return;
    }

    reader.readNextStartElement();
    if (reader.name() != QLatin1String("AppReceipt")) {
        qCDebug(lcPurchasingBackend) << "Expected AppReceipt in receipt, got:" << reader.name();
        return;
    }

    const QString appReceipt = createStringForSubReceipt(reader);

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.attributes().hasAttribute(QLatin1String("ProductId"))) {
            const QString id = reader.attributes().value(QLatin1String("ProductId")).toString();
            if (d->nativeProducts.contains(id)) {
                qCDebug(lcPurchasingBackend) << "Restoring:" << id;

                QInAppProduct *product = store()->registeredProduct(id);
                if (!product) {
                    qCDebug(lcPurchasingBackend) << "Product " << id << "has been bought, but is unknown";
                    continue;
                }


                const QString receipt = createStringForSubReceipt(reader);

                auto transaction = new QWinRTInAppTransaction(QInAppTransaction::PurchaseRestored,
                                                              product,
                                                              QInAppTransaction::NoFailure,
                                                              receipt,
                                                              this);

                emit transactionReady(transaction);
            }
        }
    }

    ComPtr<ILicenseInformation> appLicense;
    hr = d->m_bridge.get_LicenseInformation(appLicense);
    Q_ASSERT_SUCCEEDED(hr);
    boolean active;
    hr = appLicense->get_IsActive(&active);
    Q_ASSERT_SUCCEEDED(hr);
    boolean trial;
    hr = appLicense->get_IsTrial(&trial);
    Q_ASSERT_SUCCEEDED(hr);
    if (active && !trial) {
        qCDebug(lcPurchasingBackend) << "Restoring app product";

        QInAppProduct *product = store()->registeredProduct(qt_win_app_identifier);
        // App is special and needs explicit registration
        if (!product) {
            queryProduct(QInAppProduct::Unlockable, qt_win_app_identifier);
            product = store()->registeredProduct(qt_win_app_identifier);
        }

        auto transaction = new QWinRTInAppTransaction(QInAppTransaction::PurchaseRestored,
                                                      product,
                                                      QInAppTransaction::NoFailure,
                                                      appReceipt,
                                                      this);
        emit transactionReady(transaction);
    }

    // Using GetProductReceiptAsync is the better solution as one can
    // query for specific products instead of parsing through a xml.
    // However, this returns E_NOTIMPL when using the ICurrentAppSimulator,
    // so it could not be used during development phase.
#if 0
    const QStringList keys = d->nativeProducts.keys();
    for (auto item : keys) {
        HRESULT hr;
        HString productId;

        hr = productId.Set(reinterpret_cast<LPCWSTR>(item.utf16()), item.size());

        ComPtr<IAsyncOperation<HSTRING>> op;
        hr = d->m_app->GetProductReceiptAsync(productId.Get(), &op);

        if (FAILED(hr)) {
            qCDebug(lcPurchasingBackend) << "No receipt available for:" << item;
            continue;
        }

        HString receiptString;
        hr = QWinRTFunctions::await(op, receiptString.GetAddressOf());

        if (FAILED(hr)) {
            qCDebug(lcPurchasingBackend) << "Failed to wait for receipt:" << item;
            continue;
        }

        quint32 length;
        const QString receipt = hStringToQString(receiptString);
        qDebug() << "Received receipt:" << receipt;

        // Create new transaction with status == Restored and emit
        //emit transactionReady();
    }
#endif
}

void QWinRTInAppPurchaseBackend::setPlatformProperty(const QString &propertyName, const QString &value)
{
    qCDebug(lcPurchasingBackend) << __FUNCTION__ << ":" << propertyName << ":" << value;
}

void QWinRTInAppPurchaseBackend::queryProducts(const QList<Product> &products)
{
    qCDebug(lcPurchasingBackend) << __FUNCTION__ << " Size:" << products.size();

    for (auto p : products)
        queryProduct(p.productType, p.identifier);
}

void QWinRTInAppPurchaseBackend::queryProduct(QInAppProduct::ProductType productType,
                                                const QString &identifier)
{
    Q_D(QWinRTInAppPurchaseBackend);
    qCDebug(lcPurchasingBackend) << __FUNCTION__ << ":" << productType << ":" << identifier;

    if (!d->nativeProducts.contains(identifier) || !compareProductTypes(productType, d->nativeProducts.value(identifier)->type)) {
        qCDebug(lcPurchasingBackend) << "No native product called:" << identifier;
        emit productQueryFailed(productType, identifier);
        return;
    }

    ComPtr<IProductLicense> productLicense = d->findProductLicense(identifier);
    if (!productLicense && identifier != qt_win_app_identifier) {
        qCDebug(lcPurchasingBackend) << "Could not find product license even though available in listing:" << identifier;
        emit productQueryFailed(productType, identifier);
        return;
    }

    NativeProductInfo *cachedInfo = d->nativeProducts.value(identifier);
    const QString price = hStringToQString(cachedInfo->formatPrice);
    const QString name = hStringToQString(cachedInfo->productName);
    QWinRTInAppProduct *appProduct = new QWinRTInAppProduct(this,
                                                            price,
                                                            name,
                                                            QString(),
                                                            productType,
                                                            identifier,
                                                            this);

    emit productQueryDone(appProduct);
}

void QWinRTInAppPurchaseBackend::purchaseProduct(QWinRTInAppProduct *product)
{
    qCDebug(lcPurchasingBackend) << __FUNCTION__ << product;
    Q_D(QWinRTInAppPurchaseBackend);
    HRESULT hr;

    HString productId;
    hr = productId.Set(reinterpret_cast<LPCWSTR>(product->identifier().utf16()),
                       product->identifier().size());
    Q_ASSERT_SUCCEEDED(hr);

    if (product->identifier() == qt_win_app_identifier) {
        // Buy the app itself
        hr = QEventDispatcherWinRT::runOnXamlThread([d, product, this]() {
            HRESULT hr;
            ComPtr<IAsyncOperation<HSTRING>> appOp;
            hr = d->m_bridge.RequestAppPurchaseAsync(true, appOp);
            Q_ASSERT_SUCCEEDED(hr);
            auto purchaseCallback = Callback<IAsyncOperationCompletedHandler<HSTRING>>([d, product, this](IAsyncOperation<HSTRING> *op, AsyncStatus status)
            {
                HString receiptH;
                QString receiptQ;
                HRESULT hr;
                hr = op->GetResults(receiptH.GetAddressOf());
                if (SUCCEEDED(hr))
                    receiptQ = hStringToQString(receiptH);
                else
                    qWarning("Could not receive transaction receipt.");

                qt_WinRTTransactionData tData(status, product, receiptQ);
                QMetaObject::invokeMethod(this, "createTransactionDelayed", Qt::QueuedConnection,
                                          Q_ARG(qt_WinRTTransactionData, tData));

                return S_OK;
            });
            hr = appOp->put_Completed(purchaseCallback.Get());
            Q_ASSERT_SUCCEEDED(hr);
            return S_OK;
        });
    } else if (product->productType() == QInAppProduct::Unlockable) {
        hr = QEventDispatcherWinRT::runOnXamlThread([d, product, &productId, this]() {
            ComPtr<IAsyncOperation<HSTRING>> purchaseOp;
            HRESULT hr;
            hr = d->m_bridge.RequestProductPurchaseAsync(productId.Get(), true, purchaseOp);
            Q_ASSERT_SUCCEEDED(hr);
            auto purchaseCallback = Callback<IAsyncOperationCompletedHandler<HSTRING>>([d, product, this](IAsyncOperation<HSTRING> *op, AsyncStatus status)
            {
                HString receiptH;
                QString receiptQ;
                HRESULT hr;
                hr = op->GetResults(receiptH.GetAddressOf());
                if (SUCCEEDED(hr))
                    receiptQ = hStringToQString(receiptH);
                else
                    qWarning("Could not receive transaction receipt.");

                qt_WinRTTransactionData tData(status, product, receiptQ);
                QMetaObject::invokeMethod(this, "createTransactionDelayed", Qt::QueuedConnection,
                                          Q_ARG(qt_WinRTTransactionData, tData));

                return S_OK;
            });
            hr = purchaseOp->put_Completed(purchaseCallback.Get());
            Q_ASSERT_SUCCEEDED(hr);
            return S_OK;
        });
    } else {
        hr = QEventDispatcherWinRT::runOnXamlThread([d, product, &productId, this]() {
            HRESULT hr;

            ComPtr<IAsyncOperation<PurchaseResults*>> purchaseOp;
            hr = d->m_bridge.RequestProductPurchaseWithResultsAsync(productId.Get(), purchaseOp);
            Q_ASSERT_SUCCEEDED(hr);

            auto purchaseCallback = Callback<IAsyncOperationCompletedHandler<PurchaseResults*>>([d, product, this](IAsyncOperation<PurchaseResults*> *op, AsyncStatus status)
            {
                ComPtr<IPurchaseResults> purchaseResults;
                QString receiptQ;

                if (status == AsyncStatus::Completed) {
                    HRESULT hr;
                    hr = op->GetResults(&purchaseResults);
                    Q_ASSERT_SUCCEEDED(hr);
                    HString receiptH;
                    hr = purchaseResults->get_ReceiptXml(receiptH.GetAddressOf());
                    Q_ASSERT_SUCCEEDED(hr);
                    receiptQ = hStringToQString(receiptH);
                }

                qt_WinRTTransactionData tData(status, product, receiptQ, purchaseResults);
                QMetaObject::invokeMethod(this, "createTransactionDelayed", Qt::QueuedConnection,
                                          Q_ARG(qt_WinRTTransactionData, tData));

                return S_OK;
            });

            hr = purchaseOp->put_Completed(purchaseCallback.Get());
            Q_ASSERT_SUCCEEDED(hr);
            return S_OK;
        });
    }
}

void QWinRTInAppPurchaseBackend::fulfillConsumable(QWinRTInAppTransaction *transaction)
{
    Q_D(QWinRTInAppPurchaseBackend);
    qCDebug(lcPurchasingBackend) << __FUNCTION__ << transaction;
    HRESULT hr;

    GUID transactionId;
    hr = transaction->m_purchaseResults->get_TransactionId(&transactionId);
    Q_ASSERT_SUCCEEDED(hr);

    HString productId;
    const QString identifier = transaction->product()->identifier();
    hr = productId.Set(reinterpret_cast<LPCWSTR>(identifier.utf16()), identifier.size());
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IAsyncOperation<FulfillmentResult>> op;
    hr = d->m_bridge.ReportConsumableFulfillmentAsync(productId.Get(), transactionId, op);
    RETURN_VOID_IF_FAILED("Could not report consumable to be fulfilled.");

    FulfillmentResult res;
    hr = QWinRTFunctions::await(op, &res);
    RETURN_VOID_IF_FAILED("Operation to fulfill consumable failed.");

    qCDebug(lcPurchasingBackend) << "Fulfilled:" << (res == FulfillmentResult_Succeeded);
}

HRESULT QWinRTInAppPurchaseBackendPrivate::onListingInformation(IAsyncOperation<ListingInformation *> *args,
                                                         AsyncStatus status)
{
    Q_UNUSED(status);
    Q_Q(QWinRTInAppPurchaseBackend);

    qCDebug(lcPurchasingBackend) << __FUNCTION__;

    ComPtr<IListingInformation> info;
    HRESULT hr = args->GetResults(&info);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IMapView<HSTRING, ABI::Windows::ApplicationModel::Store::ProductListing *>> productListings;
    hr = info->get_ProductListings(&productListings);

    if (FAILED(hr)) {
        qCDebug(lcPurchasingBackend) << "Could not get IMapView";
        return S_OK;
    }

    typedef Collections::IKeyValuePair<HSTRING, ProductListing *> ValueItem;
    typedef Collections::IIterable<ValueItem *> ValueIterable;
    typedef Collections::IIterator<ValueItem *> ValueIterator;

    ComPtr<ValueIterable> iterable;
    ComPtr<ValueIterator> iterator;

    hr = productListings.As(&iterable);
    if (FAILED(hr))
        return S_OK;

    boolean current = false;
    hr = iterable->First(&iterator);
    if (FAILED(hr))
        return S_OK;
    hr = iterator->get_HasCurrent(&current);
    if (FAILED(hr))
        return S_OK;

    while (SUCCEEDED(hr) && current){
        ComPtr<ValueItem> currentItem;
        hr = iterator->get_Current(&currentItem);
        if (FAILED(hr)) {
            qCDebug(lcPurchasingBackend) << "Could not get currentItem:" << hr;
            continue;
        }

        HString nativeKey;
        QString productKey;
        ComPtr<IProductListing> value;
        hr = currentItem->get_Key(nativeKey.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        productKey = hStringToQString(nativeKey);
        qCDebug(lcPurchasingBackend) << "ProductKey:" << productKey;
        hr = currentItem->get_Value(&value);
        Q_ASSERT_SUCCEEDED(hr);

        auto nativeInfo = new NativeProductInfo;

        hr = value->get_ProductId(nativeInfo->productID.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        hr = value->get_FormattedPrice(nativeInfo->formatPrice.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        hr = value->get_Name(nativeInfo->productName.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IProductListingWithConsumables> converted;
        hr = value.As(&converted);
        Q_ASSERT_SUCCEEDED(hr);
        hr = converted->get_ProductType(&nativeInfo->type);
        Q_ASSERT_SUCCEEDED(hr);

        qCDebug(lcPurchasingBackend) << "Detailed info:"
                                 << " ID:" << QString::fromWCharArray(nativeInfo->productID.GetRawBuffer(nullptr))
                                 << " Price:" << QString::fromWCharArray(nativeInfo->formatPrice.GetRawBuffer(nullptr))
                                 << " Name:" << QString::fromWCharArray(nativeInfo->productName.GetRawBuffer(nullptr))
                                 << " Type:" << (nativeInfo->type == ProductType_Consumable ? QLatin1String("Consumable") : QLatin1String("Unlockable"));

        nativeProducts.insert(QString::fromWCharArray(nativeInfo->productID.GetRawBuffer(nullptr)), nativeInfo);

        hr = iterator->MoveNext(&current);
        Q_ASSERT_SUCCEEDED(hr);
    }

    ComPtr<ILicenseInformation> appLicense;
    hr = m_bridge.get_LicenseInformation(appLicense);
    Q_ASSERT_SUCCEEDED(hr);
    boolean active;
    hr = appLicense->get_IsActive(&active);
    Q_ASSERT_SUCCEEDED(hr);
    boolean trial;
    hr = appLicense->get_IsTrial(&trial);
    Q_ASSERT_SUCCEEDED(hr);
    if (active) {
        auto appInfo = new NativeProductInfo;
        hr = appInfo->productID.Set(reinterpret_cast<LPCWSTR>(qt_win_app_identifier.utf16()), qt_win_app_identifier.size());
        Q_ASSERT_SUCCEEDED(hr);
        // We store trial information inside the Name itself, as a kind of
        // app State, as the public API does not support trial mode
        const QString licenseType = trial ? QLatin1String("Trial period") : QLatin1String("Fully licensed");
        appInfo->productName.Set(reinterpret_cast<LPCWSTR>(licenseType.utf16()), licenseType.size());
        appInfo->type = ProductType_Durable;
        nativeProducts.insert(qt_win_app_identifier, appInfo);
        qCDebug(lcPurchasingBackend) << "App license valid, adding to iap items";
    }

    m_waitingForList = false;
    emit q->ready();

    return S_OK;
}

ComPtr<IProductLicense> QWinRTInAppPurchaseBackendPrivate::findProductLicense(const QString &identifier)
{
    ComPtr<ILicenseInformation> licenseInfo;
    HRESULT hr;
    hr = m_bridge.get_LicenseInformation(licenseInfo);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IMapView<HSTRING,ABI::Windows::ApplicationModel::Store::ProductLicense*>> licenses;
    hr = licenseInfo->get_ProductLicenses(&licenses);
    Q_ASSERT_SUCCEEDED(hr);

    typedef Collections::IKeyValuePair<HSTRING, ProductLicense *> ValueItem;
    typedef Collections::IIterable<ValueItem *> ValueIterable;
    typedef Collections::IIterator<ValueItem *> ValueIterator;

    ComPtr<ValueIterable> iterable;
    ComPtr<ValueIterator> iterator;

    hr = licenses.As(&iterable);
    Q_ASSERT_SUCCEEDED(hr);
    boolean current = false;
    hr = iterable->First(&iterator);
    Q_ASSERT_SUCCEEDED(hr);
    hr = iterator->get_HasCurrent(&current);
    Q_ASSERT_SUCCEEDED(hr);

    while (SUCCEEDED(hr) && current) {
        ComPtr<ValueItem> currentItem;
        hr = iterator->get_Current(&currentItem);
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IProductLicense> productLicense;
        hr = currentItem->get_Value(&productLicense);
        Q_ASSERT_SUCCEEDED(hr);

        HString productId;
        hr = productLicense->get_ProductId(productId.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);

        quint32 length;
        QString qProductId = QString::fromWCharArray(productId.GetRawBuffer(&length));
        if (qProductId == identifier) {
            return productLicense;
        }
        hr = iterator->MoveNext(&current);
        Q_ASSERT_SUCCEEDED(hr);
    }
    return ComPtr<IProductLicense>();
}

QT_END_NAMESPACE
