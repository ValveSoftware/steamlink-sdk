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

#include "qwebview_p.h"
#include "qwebview_osx_p.h"
#include "qwebviewloadrequest_p.h"

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#import <CoreFoundation/CoreFoundation.h>
#import <WebKit/WebKit.h>

#if QT_MACOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_11)
#define QtFrameLoadDelegateProtocol <WebFrameLoadDelegate>
#else
// WebFrameLoadDelegate is an informal protocol in <= 10.10 SDK.
#define QtFrameLoadDelegateProtocol
#endif

@interface QtFrameLoadDelegate : NSObject QtFrameLoadDelegateProtocol {
    QOsxWebViewPrivate *qtWebViewPrivate;
}
- (QtFrameLoadDelegate *)initWithQWebViewPrivate:(QOsxWebViewPrivate *)webViewPrivate;
- (void)pageDone;

// protocol:
- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
@end

@implementation QtFrameLoadDelegate

- (QtFrameLoadDelegate *)initWithQWebViewPrivate:(QOsxWebViewPrivate *)webViewPrivate
{
    qtWebViewPrivate = webViewPrivate;
    return self;
}

- (void)pageDone
{
    Q_EMIT qtWebViewPrivate->titleChanged(qtWebViewPrivate->title());
    Q_EMIT qtWebViewPrivate->loadProgressChanged(qtWebViewPrivate->loadProgress());
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    Q_UNUSED(sender);
    Q_UNUSED(frame);

    // WebView gives us per-frame notifications while the QWebView API
    // should provide per-page notifications. Keep track of started frame loads
    // and emit notifications when the final frame completes.
    if (++qtWebViewPrivate->requestFrameCount == 1) {
        Q_EMIT qtWebViewPrivate->loadingChanged(QWebViewLoadRequestPrivate(qtWebViewPrivate->url(),
                                                                           QWebView::LoadStartedStatus,
                                                                           QString()));
    }
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    Q_UNUSED(sender);
    Q_UNUSED(frame);
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    Q_UNUSED(sender);
    Q_UNUSED(frame);
    if (--qtWebViewPrivate->requestFrameCount == 0) {
        [self pageDone];
        Q_EMIT qtWebViewPrivate->loadingChanged(QWebViewLoadRequestPrivate(qtWebViewPrivate->url(),
                                                                           QWebView::LoadSucceededStatus,
                                                                           QString()));
    }
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    Q_UNUSED(sender);
    Q_UNUSED(error);
    Q_UNUSED(frame);
    if (--qtWebViewPrivate->requestFrameCount == 0) {
        [self pageDone];
        NSString *errorString = [error localizedFailureReason];
        Q_EMIT qtWebViewPrivate->loadingChanged(QWebViewLoadRequestPrivate(qtWebViewPrivate->url(),
                                                                           QWebView::LoadFailedStatus,
                                                                           QString::fromNSString(errorString)));
    }
}

@end

QT_BEGIN_NAMESPACE

QOsxWebViewPrivate::QOsxWebViewPrivate(QWebView *q)
    : QWebViewPrivate(q)
{
    NSRect frame = NSMakeRect(0.0, 0.0, 400, 400);
    webView = [[WebView alloc] initWithFrame:frame frameName:@"Qt Frame" groupName:nil];
    [webView setFrameLoadDelegate:[[QtFrameLoadDelegate alloc] initWithQWebViewPrivate:this]];
    m_window = QWindow::fromWinId(reinterpret_cast<WId>(webView));
}

QOsxWebViewPrivate::~QOsxWebViewPrivate()
{
    [webView.frameLoadDelegate release];
    [webView release];
    if (m_window != 0 && m_window->parent() == 0)
        delete m_window;
}

QUrl QOsxWebViewPrivate::url() const
{
    NSString *currentURL = [webView stringByEvaluatingJavaScriptFromString:@"window.location.href"];
    return QString::fromNSString(currentURL);
}

void QOsxWebViewPrivate::setUrl(const QUrl &url)
{
    requestFrameCount = 0;
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:url.toNSURL()]];
}

bool QOsxWebViewPrivate::canGoBack() const
{
    return webView.canGoBack;
}

void QOsxWebViewPrivate::goBack()
{
    [webView goBack];
}

bool QOsxWebViewPrivate::canGoForward() const
{
    return webView.canGoForward;
}

void QOsxWebViewPrivate::goForward()
{
    [webView goForward];
}

void QOsxWebViewPrivate::reload()
{
    [[webView mainFrame] reload];
}

QString QOsxWebViewPrivate::title() const
{
    NSString *title = [webView stringByEvaluatingJavaScriptFromString:@"document.title"];
    return QString::fromNSString(title);
}

int QOsxWebViewPrivate::loadProgress() const
{
    // TODO:
    return isLoading() ? 100 : 0;
}

bool QOsxWebViewPrivate::isLoading() const
{
    return webView.isLoading;
}

void QOsxWebViewPrivate::runJavaScriptPrivate(const QString& script, int callbackId)
{
    // ### TODO needs more async
    NSString *result = [webView stringByEvaluatingJavaScriptFromString:script.toNSString()];
    if (callbackId != -1)
        Q_EMIT javaScriptResult(callbackId, QVariant::fromValue(QString::fromNSString(result)));
}

void QOsxWebViewPrivate::setParentView(QObject *view)
{
    m_window->setParent(qobject_cast<QWindow *>(view));
}

QObject *QOsxWebViewPrivate::parentView() const
{
    return m_window->parent();
}

void QOsxWebViewPrivate::setGeometry(const QRect &geometry)
{
    m_window->setGeometry(geometry);
}

void QOsxWebViewPrivate::setVisibility(QWindow::Visibility visibility)
{
    m_window->setVisibility(visibility);
}

void QOsxWebViewPrivate::setVisible(bool visible)
{
    m_window->setVisible(visible);
}

void QOsxWebViewPrivate::stop()
{
    [webView stopLoading:webView];
}

void QOsxWebViewPrivate::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_UNUSED(html);
    Q_UNUSED(baseUrl);
}

QT_END_NAMESPACE
