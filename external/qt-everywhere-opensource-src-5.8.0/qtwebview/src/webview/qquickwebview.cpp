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

#include "qquickwebview_p.h"
#include "qquickwebviewloadrequest_p.h"
#include <QtWebView/private/qwebviewloadrequest_p.h>
#include <QtQml/qqmlengine.h>
#include <QtCore/qmutex.h>

namespace {

class CallbackStorage
{
public:
    int insertCallback(const QJSValue &callback)
    {
        QMutexLocker locker(&m_mtx);
        const int nextId = qMax(++m_counter, 0);
        if (nextId == 0)
          m_counter = 1;

        m_callbacks.insert(nextId, callback);
        return nextId;
    }

    QJSValue takeCallback(int callbackId)
    {
        QMutexLocker lock(&m_mtx);
        return m_callbacks.take(callbackId);
    }

private:
    QMutex m_mtx;
    int m_counter;
    QHash<int, QJSValue> m_callbacks;
};

} // namespace

Q_GLOBAL_STATIC(CallbackStorage, callbacks)

/*!
    \qmltype WebView
    \inqmlmodule QtWebView
    \ingroup qtwebview
    \brief A component for displaying web content

    WebView is a component for displaying web content which is implemented using native
    APIs on the platforms where this is available, thus it does not necessarily require
    including a full web browser stack as part of the application.

    To make the Qt WebView module function correctly across all platforms, it is necessary
    to call \l {qtwebview-initialize}{QtWebView::initialize}() right after creating the
    QGuiApplication instance.

    \note Due to platform limitations overlapping the WebView and other QML components
    is not supported.
*/

QQuickWebView::QQuickWebView(QQuickItem *parent)
    : QQuickViewController(parent)
    , m_webView(new QWebView(this))
{
    setView(m_webView);
    connect(m_webView, &QWebView::titleChanged, this, &QQuickWebView::titleChanged);
    connect(m_webView, &QWebView::urlChanged, this, &QQuickWebView::urlChanged);
    connect(m_webView, &QWebView::loadProgressChanged, this, &QQuickWebView::loadProgressChanged);
    connect(m_webView, &QWebView::loadingChanged, this, &QQuickWebView::onLoadingChanged);
    connect(m_webView, &QWebView::requestFocus, this, &QQuickWebView::onFocusRequest);
    connect(m_webView, &QWebView::javaScriptResult, this, &QQuickWebView::onRunJavaScriptResult);
}

QQuickWebView::~QQuickWebView()
{
}

/*!
  \qmlproperty url QtWebView::WebView::url

  The URL of currently loaded web page. Changing this will trigger
  loading new content.

  The URL is used as-is. URLs that originate from user input should
  be parsed with QUrl::fromUserInput().

  \note The WebView does not support loading content through the Qt Resource system.
*/

void QQuickWebView::setUrl(const QUrl &url)
{
    m_webView->setUrl(url);
}

/*!
  \qmlproperty string QtWebView::WebView::title
  \readonly

  The title of the currently loaded web page.
*/

QString QQuickWebView::title() const
{
    return m_webView->title();
}

QUrl QQuickWebView::url() const
{
    return m_webView->url();
}

/*!
  \qmlproperty bool QtWebView::WebView::canGoBack
  \readonly

  Holds \c true if it's currently possible to navigate back in the web history.
*/

bool QQuickWebView::canGoBack() const
{
    return m_webView->canGoBack();
}

/*!
  \qmlproperty bool QtWebView::WebView::canGoForward
  \readonly

  Holds \c true if it's currently possible to navigate forward in the web history.
*/

bool QQuickWebView::canGoForward() const
{
    return m_webView->canGoForward();
}

/*!
  \qmlproperty int QtWebView::WebView::loadProgress
  \readonly

  The current load progress of the web content, represented as
  an integer between 0 and 100.
*/
int QQuickWebView::loadProgress() const
{
    return m_webView->loadProgress();
}

/*!
  \qmlproperty bool QtWebView::WebView::loading
  \readonly

  Holds \c true if the WebView is currently in the process of loading
  new content, \c false otherwise.

  \sa loadingChanged()
*/

/*!
  \qmlsignal QtWebView::WebView::loadingChanged(WebViewLoadRequest loadRequest)

  This signal is emitted when the state of loading the web content changes.
  By handling this signal it's possible, for example, to react to page load
  errors.

  The \a loadRequest parameter holds the \e url and \e status of the request,
  as well as an \e errorString containing an error message for a failed
  request.

  \sa WebViewLoadRequest
*/
bool QQuickWebView::isLoading() const
{
    return m_webView->isLoading();
}

/*!
    \qmlmethod void QtWebView::WebView::goBack()

    Navigates back in the web history.
*/
void QQuickWebView::goBack()
{
    m_webView->goBack();
}

/*!
    \qmlmethod void QtWebView::WebView::goForward()

    Navigates forward in the web history.
*/
void QQuickWebView::goForward()
{
    m_webView->goForward();
}

/*!
    \qmlmethod void QtWebView::WebView::reload()

    Reloads the current \l url.
*/
void QQuickWebView::reload()
{
    m_webView->reload();
}

/*!
    \qmlmethod void QtWebView::WebView::stop()

    Stops loading the current \l url.
*/
void QQuickWebView::stop()
{
    m_webView->stop();
}

/*!
    \qmlmethod void QtWebView::WebView::loadHtml(string html, url baseUrl)

    Loads the specified \a html content to the web view.

    This method offers a lower-level alternative to the \l url property,
    which references HTML pages via URL.

    External objects such as stylesheets or images referenced in the HTML
    document should be located relative to \a baseUrl. For example, if \a html
    is retrieved from \c http://www.example.com/documents/overview.html, which
    is the base URL, then an image referenced with the relative url, \c diagram.png,
    should be at \c{http://www.example.com/documents/diagram.png}.

    \note The WebView does not support loading content through the Qt Resource system.

    \sa url
*/
void QQuickWebView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    m_webView->loadHtml(html, baseUrl);
}

/*!
    \qmlmethod void QtWebView::WebView::runJavaScript(string script, variant callback)

    Runs the specified JavaScript.
    In case a \a callback function is provided, it will be invoked after the \a script
    finished running.

    \badcode
    runJavaScript("document.title", function(result) { console.log(result); });
    \endcode
*/
void QQuickWebView::runJavaScript(const QString &script, const QJSValue &callback)
{
    const int callbackId = callback.isCallable() ? callbacks->insertCallback(callback)
                                                 : -1;
    runJavaScriptPrivate(script, callbackId);
}

void QQuickWebView::runJavaScriptPrivate(const QString &script, int callbackId)
{
    m_webView->runJavaScriptPrivate(script, callbackId);
}

void QQuickWebView::onRunJavaScriptResult(int id, const QVariant &variant)
{
    if (id == -1)
        return;

    QJSValue callback = callbacks->takeCallback(id);
    if (callback.isUndefined())
        return;

    QQmlEngine *engine = qmlEngine(this);
    if (engine == 0) {
        qWarning() << "No JavaScript engine, unable to handle JavaScript callback!";
        return;
    }

    QJSValueList args;
    args.append(engine->toScriptValue(variant));
    callback.call(args);
}

void QQuickWebView::onFocusRequest(bool focus)
{
    setFocus(focus);
}

void QQuickWebView::onLoadingChanged(const QWebViewLoadRequestPrivate &loadRequest)
{
    QQuickWebViewLoadRequest qqLoadRequest(loadRequest);
    Q_EMIT loadingChanged(&qqLoadRequest);
}

QJSValue QQuickWebView::takeCallback(int id)
{
    return callbacks->takeCallback(id);
}
