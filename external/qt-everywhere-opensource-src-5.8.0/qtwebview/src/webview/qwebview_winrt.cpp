/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebView module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwebview_winrt_p.h"
#include "qwebviewloadrequest_p.h"

#include <functional>

#include <QPointer>
#include <QHash>
#include <QRegularExpression>
#include <QScreen>
#include <qfunctions_winrt.h>
#include <private/qeventdispatcher_winrt_p.h>
#include <private/qhighdpiscaling_p.h>

#include <wrl.h>
#include <windows.graphics.display.h>
#include <windows.ui.xaml.h>
#include <windows.ui.xaml.controls.h>
#include <windows.ui.xaml.markup.h>
#include <windows.web.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Graphics::Display;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::UI::Xaml;
using namespace ABI::Windows::UI::Xaml::Controls;
using namespace ABI::Windows::UI::Xaml::Markup;
using namespace ABI::Windows::Web;

class HStringListIterator : public RuntimeClass<RuntimeClassFlags<WinRt>, IIterator<HSTRING>>
{
public:
    HStringListIterator(HSTRING *data, int size) : data(data), size(size), pos(0)
    {
    }

    HRESULT __stdcall get_Current(HSTRING *current) override
    {
        *current = pos >= size ? nullptr : data[pos];
        return S_OK;
    }

    HRESULT __stdcall get_HasCurrent(boolean *hasCurrent) override
    {
        *hasCurrent = pos < size;
        return S_OK;
    }

    HRESULT __stdcall MoveNext(boolean *hasCurrent) override
    {
        *hasCurrent = ++pos < size;
        return S_OK;
    }

    HRESULT __stdcall GetMany(unsigned capacity, HSTRING *value, unsigned *actual)
    {
        unsigned i = 0;
        for (; i < qMin(capacity, unsigned(size)); ++i)
            value[i] = data[pos + i];
        *actual = i;
        pos += i;
        return S_OK;
    }

private:
    HSTRING *data;
    int size;
    int pos;
};

class HStringList : public RuntimeClass<RuntimeClassFlags<WinRt>, IIterable<HSTRING>>
{
public:
    HStringList(const QList<QString> &stringList)
    {
        d.resize(stringList.size());
        for (int i = 0; i < stringList.size(); ++i) {
            const QString qString = stringList.at(i).trimmed();
            HStringReference hString(reinterpret_cast<const wchar_t *>(qString.utf16()), qString.length());
            hString.CopyTo(&d[i++]);
        }
    }

    ~HStringList()
    {
        for (const HSTRING &hString : qAsConst(d))
            WindowsDeleteString(hString);
    }

    HRESULT __stdcall First(IIterator<HSTRING> **first) override
    {
        ComPtr<HStringListIterator> it = Make<HStringListIterator>(d.data(), d.size());
        return it.Get()->QueryInterface(IID_PPV_ARGS(first));
    }

private:
    QVector<HSTRING> d;
};

static QUrl qurlFromUri(IUriRuntimeClass *uri)
{
    HRESULT hr;
    HString uriString;
    hr = uri->get_AbsoluteUri(uriString.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    quint32 uriStringLength;
    const wchar_t *uriStringBuffer = uriString.GetRawBuffer(&uriStringLength);
    return QUrl(QString::fromWCharArray(uriStringBuffer, uriStringLength));
}

static QString webErrorStatusString(WebErrorStatus status)
{
    switch (status) {
    case WebErrorStatus_Unknown:
        return QStringLiteral("Unknown");
    case WebErrorStatus_CertificateCommonNameIsIncorrect:
        return QStringLiteral("CertificateCommonNameIsIncorrect");
    case WebErrorStatus_CertificateExpired:
        return QStringLiteral("CertificateExpired");
    case WebErrorStatus_CertificateContainsErrors:
        return QStringLiteral("CertificateContainsErrors");
    case WebErrorStatus_CertificateRevoked:
        return QStringLiteral("CertificateRevoked");
    case WebErrorStatus_CertificateIsInvalid:
        return QStringLiteral("CertificateIsInvalid");
    case WebErrorStatus_ServerUnreachable:
        return QStringLiteral("ServerUnreachable");
    case WebErrorStatus_Timeout:
        return QStringLiteral("Timeout");
    case WebErrorStatus_ErrorHttpInvalidServerResponse:
        return QStringLiteral("ErrorHttpInvalidServerResponse");
    case WebErrorStatus_ConnectionAborted:
        return QStringLiteral("ConnectionAborted");
    case WebErrorStatus_ConnectionReset:
        return QStringLiteral("ConnectionReset");
    case WebErrorStatus_Disconnected:
        return QStringLiteral("Disconnected");
    case WebErrorStatus_HttpToHttpsOnRedirection:
        return QStringLiteral("HttpToHttpsOnRedirection");
    case WebErrorStatus_HttpsToHttpOnRedirection:
        return QStringLiteral("HttpsToHttpOnRedirection");
    case WebErrorStatus_CannotConnect:
        return QStringLiteral("CannotConnect");
    case WebErrorStatus_HostNameNotResolved:
        return QStringLiteral("HostNameNotResolved");
    case WebErrorStatus_OperationCanceled:
        return QStringLiteral("OperationCanceled");
    case WebErrorStatus_RedirectFailed:
        return QStringLiteral("RedirectFailed");
    case WebErrorStatus_UnexpectedStatusCode:
        return QStringLiteral("UnexpectedStatusCode");
    case WebErrorStatus_UnexpectedRedirection:
        return QStringLiteral("UnexpectedRedirection");
    case WebErrorStatus_UnexpectedClientError:
        return QStringLiteral("UnexpectedClientError");
    case WebErrorStatus_UnexpectedServerError:
        return QStringLiteral("UnexpectedServerError");
    case WebErrorStatus_MultipleChoices:
        return QStringLiteral("MultipleChoices");
    case WebErrorStatus_MovedPermanently:
        return QStringLiteral("MovedPermanently");
    case WebErrorStatus_Found:
        return QStringLiteral("Found");
    case WebErrorStatus_SeeOther:
        return QStringLiteral("SeeOther");
    case WebErrorStatus_NotModified:
        return QStringLiteral("NotModified");
    case WebErrorStatus_UseProxy:
        return QStringLiteral("UseProxy");
    case WebErrorStatus_TemporaryRedirect:
        return QStringLiteral("TemporaryRedirect");
    case WebErrorStatus_BadRequest:
        return QStringLiteral("BadRequest");
    case WebErrorStatus_Unauthorized:
        return QStringLiteral("Unauthorized");
    case WebErrorStatus_PaymentRequired:
        return QStringLiteral("PaymentRequired");
    case WebErrorStatus_Forbidden:
        return QStringLiteral("Forbidden");
    case WebErrorStatus_NotFound:
        return QStringLiteral("NotFound");
    case WebErrorStatus_MethodNotAllowed:
        return QStringLiteral("MethodNotAllowed");
    case WebErrorStatus_NotAcceptable:
        return QStringLiteral("NotAcceptable");
    case WebErrorStatus_ProxyAuthenticationRequired:
        return QStringLiteral("ProxyAuthenticationRequired");
    case WebErrorStatus_RequestTimeout:
        return QStringLiteral("RequestTimeout");
    case WebErrorStatus_Conflict:
        return QStringLiteral("Conflict");
    case WebErrorStatus_Gone:
        return QStringLiteral("Gone");
    case WebErrorStatus_LengthRequired:
        return QStringLiteral("LengthRequired");
    case WebErrorStatus_PreconditionFailed:
        return QStringLiteral("PreconditionFailed");
    case WebErrorStatus_RequestEntityTooLarge:
        return QStringLiteral("RequestEntityTooLarge");
    case WebErrorStatus_RequestUriTooLong:
        return QStringLiteral("RequestUriTooLong");
    case WebErrorStatus_UnsupportedMediaType:
        return QStringLiteral("UnsupportedMediaType");
    case WebErrorStatus_RequestedRangeNotSatisfiable:
        return QStringLiteral("RequestedRangeNotSatisfiable");
    case WebErrorStatus_ExpectationFailed:
        return QStringLiteral("ExpectationFailed");
    case WebErrorStatus_InternalServerError:
        return QStringLiteral("InternalServerError");
    case WebErrorStatus_NotImplemented:
        return QStringLiteral("NotImplemented");
    case WebErrorStatus_BadGateway:
        return QStringLiteral("BadGateway");
    case WebErrorStatus_ServiceUnavailable:
        return QStringLiteral("ServiceUnavailable");
    case WebErrorStatus_GatewayTimeout:
        return QStringLiteral("GatewayTimeout");
    case WebErrorStatus_HttpVersionNotSupported:
        return QStringLiteral("HttpVersionNotSupported");
    default:
        break;
    }
    return QString();
}

QWebViewPrivate *QWebViewPrivate::create(QWebView *q)
{
    return new QWinRTWebViewPrivate(q);
}

struct WinRTWebView
{
    ComPtr<IWebView> base;
    ComPtr<IWebView2> ext;
    ComPtr<IPanel> host;

    ComPtr<ICanvasStatics> canvas;
    ComPtr<IUriRuntimeClassFactory> uriFactory;
    ComPtr<IDisplayInformation> displayInformation;

    QPointer<QWindow> window;
    QHash<IAsyncOperation<HSTRING> *, int> callbacks;

    EventRegistrationToken navigationStartingToken;
    EventRegistrationToken navigationCompletedToken;
    bool isLoading : 1;
};

#define LSTRING(str) L#str
static const wchar_t webviewXaml[] = LSTRING(
<WebView xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" />
);

QWinRTWebViewPrivate::QWinRTWebViewPrivate(QObject *parent)
    : QWebViewPrivate(parent), d(new WinRTWebView)
{
    d->isLoading = false;

    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        ComPtr<IXamlReaderStatics> xamlReader;
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_UI_Xaml_Markup_XamlReader).Get(),
                                    IID_PPV_ARGS(&xamlReader));
        Q_ASSERT_SUCCEEDED(hr);

        // Directly instantiating a WebView works, but it throws an exception
        // when navigating. Using a XamlReader appears to set it up properly.
        ComPtr<IInspectable> element;
        hr = xamlReader->Load(HString::MakeReference(webviewXaml).Get(), &element);
        Q_ASSERT_SUCCEEDED(hr);
        hr = element.As(&d->base);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->base.As(&d->ext);
        Q_ASSERT_SUCCEEDED(hr);

        hr = d->ext->add_NavigationStarting(
                    Callback<ITypedEventHandler<WebView *, WebViewNavigationStartingEventArgs *>>(this, &QWinRTWebViewPrivate::onNavigationStarted).Get(),
                    &d->navigationStartingToken);
        Q_ASSERT_SUCCEEDED(hr);
        d->ext->add_NavigationCompleted(
                    Callback<ITypedEventHandler<WebView *, WebViewNavigationCompletedEventArgs *>>(this, &QWinRTWebViewPrivate::onNavigationCompleted).Get(),
                    &d->navigationCompletedToken);
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IDisplayInformationStatics> displayInformationStatics;
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Graphics_Display_DisplayInformation).Get(),
                                    IID_PPV_ARGS(&displayInformationStatics));
        Q_ASSERT_SUCCEEDED(hr);
        hr = displayInformationStatics->GetForCurrentView(&d->displayInformation);
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });

    HRESULT hr;
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_UI_Xaml_Controls_Canvas).Get(),
                                IID_PPV_ARGS(&d->canvas));
    Q_ASSERT_SUCCEEDED(hr);
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                                IID_PPV_ARGS(&d->uriFactory));
    Q_ASSERT_SUCCEEDED(hr);
}

QWinRTWebViewPrivate::~QWinRTWebViewPrivate()
{
    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        ComPtr<IVector<UIElement *>> children;
        hr = d->host->get_Children(&children);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IUIElement> uiElement;
        hr = d->base.As(&uiElement);
        Q_ASSERT_SUCCEEDED(hr);
        quint32 index;
        boolean found;
        hr = children->IndexOf(uiElement.Get(), &index, &found);
        Q_ASSERT_SUCCEEDED(hr);
        if (found) {
            hr = children->RemoveAt(index);
            Q_ASSERT_SUCCEEDED(hr);
        }
        return hr;
    });
}

QUrl QWinRTWebViewPrivate::url() const
{
    ComPtr<IUriRuntimeClass> uri;
    QEventDispatcherWinRT::runOnXamlThread([this, &uri]() {
        HRESULT hr;
        hr = d->base->get_Source(&uri);
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
    return qurlFromUri(uri.Get());
}

void QWinRTWebViewPrivate::setUrl(const QUrl &url)
{
    QEventDispatcherWinRT::runOnXamlThread([this, &url]() {
        HRESULT hr;
        const QString urlString = url.toString();
        ComPtr<IUriRuntimeClass> uri;
        HStringReference uriString(reinterpret_cast<const wchar_t *>(urlString.utf16()),
                                   urlString.length());
        hr = d->uriFactory->CreateUri(uriString.Get(), &uri);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->base->Navigate(uri.Get());
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

bool QWinRTWebViewPrivate::canGoBack() const
{
    boolean canGoBack;
    QEventDispatcherWinRT::runOnXamlThread([this, &canGoBack]() {
        HRESULT hr;
        hr = d->ext->get_CanGoBack(&canGoBack);
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
    return canGoBack;
}

bool QWinRTWebViewPrivate::canGoForward() const
{
    boolean canGoForward;
    QEventDispatcherWinRT::runOnXamlThread([this, &canGoForward]() {
        HRESULT hr;
        hr = d->ext->get_CanGoForward(&canGoForward);
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
    return canGoForward;
}

QString QWinRTWebViewPrivate::title() const
{
    HString title;
    QEventDispatcherWinRT::runOnXamlThread([this, &title]() {
        HRESULT hr;
        hr = d->ext->get_DocumentTitle(title.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
    quint32 titleLength;
    const wchar_t *titleBuffer = title.GetRawBuffer(&titleLength);
    return QString::fromWCharArray(titleBuffer, titleLength);
}

int QWinRTWebViewPrivate::loadProgress() const
{
    return d->isLoading ? 0 : 100;
}

bool QWinRTWebViewPrivate::isLoading() const
{
    return d->isLoading;
}

void QWinRTWebViewPrivate::setParentView(QObject *view)
{
    d->window = qobject_cast<QWindow *>(view);
    if (!d->window)
        return;

    ComPtr<IInspectable> host = reinterpret_cast<IInspectable *>(d->window->winId());
    if (!host)
        return;

    QEventDispatcherWinRT::runOnXamlThread([this, &host]() {
        HRESULT hr;
        ComPtr<IFrameworkElement> frameworkHost;
        hr = host.As(&frameworkHost);
        RETURN_HR_IF_FAILED("Failed to cast the window ID to IFrameworkElement");
        ComPtr<IDependencyObject> parent;
        hr = frameworkHost->get_Parent(&parent);
        RETURN_HR_IF_FAILED("Failed to get the parent object of the window");
        hr = parent.As(&d->host);
        RETURN_HR_IF_FAILED("Failed to cast the window container to IPanel");
        ComPtr<IVector<UIElement *>> children;
        hr = d->host->get_Children(&children);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IUIElement> uiElement;
        hr = d->base.As(&uiElement);
        Q_ASSERT_SUCCEEDED(hr);
        hr = children->Append(uiElement.Get());
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

QObject *QWinRTWebViewPrivate::parentView() const
{
    return d->window;
}

void QWinRTWebViewPrivate::setGeometry(const QRect &geometry)
{
    QEventDispatcherWinRT::runOnXamlThread([this, &geometry]() {
        HRESULT hr;
        double scaleFactor;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
        ComPtr<IDisplayInformation2> displayInformation;
        hr = d->displayInformation.As(&displayInformation);
        Q_ASSERT_SUCCEEDED(hr);
        hr = displayInformation->get_RawPixelsPerViewPixel(&scaleFactor);
        Q_ASSERT_SUCCEEDED(hr);
        scaleFactor = 1.0 / scaleFactor;
        Q_ASSERT(d->window);
        const QScreen *screen = d->window->screen();
        Q_ASSERT(screen);
        const QPoint screenTopLeft = screen->availableGeometry().topLeft();
        const QPointF topLeft = QHighDpi::toNativePixels(QPointF(geometry.topLeft() + screenTopLeft) * scaleFactor, screen);
        const QSizeF size = QHighDpi::toNativePixels(QSizeF(geometry.size()) * scaleFactor, screen);
#else
        ResolutionScale resolutionScale;
        hr = d->displayInformation->get_ResolutionScale(&resolutionScale);
        Q_ASSERT_SUCCEEDED(hr);
        scaleFactor = 100.0 / double(resolutionScale);
        const QPointF topLeft = QPointF(geometry.topLeft()) * scaleFactor;
        const QSizeF size = QSizeF(geometry.size()) * scaleFactor;
#endif
        ComPtr<IUIElement> uiElement;
        hr = d->base.As(&uiElement);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->canvas->SetLeft(uiElement.Get(), topLeft.x());
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->canvas->SetTop(uiElement.Get(), topLeft.y());
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IFrameworkElement> frameworkElement;
        hr = d->base.As(&frameworkElement);
        Q_ASSERT_SUCCEEDED(hr);
        hr = frameworkElement->put_Width(size.width());
        Q_ASSERT_SUCCEEDED(hr);
        hr = frameworkElement->put_Height(size.height());
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

void QWinRTWebViewPrivate::setVisibility(QWindow::Visibility visibility)
{
    setVisible(visibility != QWindow::Hidden);
}

void QWinRTWebViewPrivate::setVisible(bool visible)
{
    QEventDispatcherWinRT::runOnXamlThread([this, &visible]() {
        HRESULT hr;
        ComPtr<IUIElement> uiElement;
        hr = d->base.As(&uiElement);
        Q_ASSERT_SUCCEEDED(hr);
        hr = uiElement->put_Visibility(visible ? Visibility_Visible : Visibility_Collapsed);
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

void QWinRTWebViewPrivate::goBack()
{
    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        hr = d->ext->GoBack();
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

void QWinRTWebViewPrivate::goForward()
{
    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        hr = d->ext->GoForward();
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

void QWinRTWebViewPrivate::reload()
{
    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        hr = d->ext->Refresh();
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

void QWinRTWebViewPrivate::stop()
{
    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        hr = d->ext->Stop();
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

void QWinRTWebViewPrivate::loadHtml(const QString &html, const QUrl &baseUrl)
{
    if (!baseUrl.isEmpty())
        qWarning("Base URLs for loadHtml() are not supported under WinRT.");

    HRESULT hr;
    HStringReference htmlText(reinterpret_cast<const wchar_t *>(html.utf16()), html.length());
    hr = d->base->NavigateToString(htmlText.Get());
    Q_ASSERT_SUCCEEDED(hr);
}

void QWinRTWebViewPrivate::runJavaScriptPrivate(const QString &script, int callbackId)
{
    static QRegularExpression functionTemplate(QStringLiteral("^(.*)\\((.*)\\)[\\s;]*?$"));
    QRegularExpressionMatch match = functionTemplate.match(script);
    if (!match.hasMatch()) {
        qWarning("The WinRT WebView only supports calling global functions, so please make your call"
                 " in the form myFunction(a, b, c). Also note that only string arguments can be passed.");
        return;
    }

    const QString method = match.captured(1).trimmed();
    HStringReference methodString(reinterpret_cast<const wchar_t *>(method.utf16()), method.length());
    ComPtr<HStringList> argumentStrings = Make<HStringList>(match.captured(2).split(QLatin1Char(',')));
    QEventDispatcherWinRT::runOnXamlThread([this, &methodString, &argumentStrings, &callbackId]() {
        HRESULT hr;
        ComPtr<IAsyncOperation<HSTRING>> op;
        hr = d->ext->InvokeScriptAsync(methodString.Get(), argumentStrings.Get(), &op);
        Q_ASSERT_SUCCEEDED(hr);

        d->callbacks.insert(op.Get(), callbackId);
        hr = op->put_Completed(Callback<IAsyncOperationCompletedHandler<HSTRING>>([this](IAsyncOperation<HSTRING> *op, AsyncStatus status) {
            int callbackId = d->callbacks.take(op);
            HRESULT hr;
            if (status != Completed) {
                ComPtr<IAsyncInfo> info;
                hr = op->QueryInterface(IID_PPV_ARGS(&info));
                Q_ASSERT_SUCCEEDED(hr);
                HRESULT errorCode;
                hr = info->get_ErrorCode(&errorCode);
                Q_ASSERT_SUCCEEDED(hr);
                emit javaScriptResult(callbackId, qt_error_string(errorCode));
                return S_OK;
            }
            HString result;
            hr = op->GetResults(result.GetAddressOf());
            Q_ASSERT_SUCCEEDED(hr);
            quint32 resultLength;
            const wchar_t *resultBuffer = result.GetRawBuffer(&resultLength);
            emit javaScriptResult(callbackId, QString::fromWCharArray(resultBuffer, resultLength));
            return S_OK;
        }).Get());
        Q_ASSERT_SUCCEEDED(hr);
        return hr;
    });
}

HRESULT QWinRTWebViewPrivate::onNavigationStarted(IWebView *, IWebViewNavigationStartingEventArgs *args)
{
    d->isLoading = true;
    HRESULT hr;
    ComPtr<IUriRuntimeClass> uri;
    hr = args->get_Uri(&uri);
    Q_ASSERT_SUCCEEDED(hr);
    const QUrl url = qurlFromUri(uri.Get());

    emit loadingChanged(QWebViewLoadRequestPrivate(url, QWebView::LoadStartedStatus, QString()));
    emit loadProgressChanged(0);
    return S_OK;
}

HRESULT QWinRTWebViewPrivate::onNavigationCompleted(IWebView *, IWebViewNavigationCompletedEventArgs *args)
{
    d->isLoading = false;
    HRESULT hr;
    ComPtr<IUriRuntimeClass> uri;
    hr = args->get_Uri(&uri);
    Q_ASSERT_SUCCEEDED(hr);
    const QUrl url = qurlFromUri(uri.Get());

    boolean isSuccess;
    hr = args->get_IsSuccess(&isSuccess);
    Q_ASSERT_SUCCEEDED(hr);
    const QWebView::LoadStatus status = isSuccess ? QWebView::LoadSucceededStatus : QWebView::LoadFailedStatus;

    WebErrorStatus errorStatus;
    hr = args->get_WebErrorStatus(&errorStatus);
    Q_ASSERT_SUCCEEDED(hr);
    const QString errorString = webErrorStatusString(errorStatus);

    emit loadingChanged(QWebViewLoadRequestPrivate(url, status, errorString));
    emit titleChanged(title());
    emit loadProgressChanged(100);
    return S_OK;
}
