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

#include "qwebview_ios_p.h"
#include "qwebview_darwin_p.h"
#include "qwebview_p.h"
#include "qwebviewloadrequest_p.h"

#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>

#include <CoreFoundation/CoreFoundation.h>
#include <UIKit/UIKit.h>

#import <UIKit/UIView.h>
#import <UIKit/UIWindow.h>
#import <UIKit/UIViewController.h>
#import <UIKit/UITapGestureRecognizer.h>
#import <UIKit/UIGestureRecognizerSubclass.h>

QT_BEGIN_NAMESPACE

static inline CGRect toCGRect(const QRectF &rect)
{
    return CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}

// -------------------------------------------------------------------------

class QWebViewInterface;

@interface QtWebViewDelegate : NSObject<UIWebViewDelegate> {
    QIosWebViewPrivate *qIosWebViewPrivate;
}
- (QtWebViewDelegate *)initWithQAbstractWebView:(QIosWebViewPrivate *)webViewPrivate;
- (void)pageDone;

// protocol:
- (void)webViewDidStartLoad:(UIWebView *)webView;
- (void)webViewDidFinishLoad:(UIWebView *)webView;
- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error;
@end

@implementation QtWebViewDelegate
- (QtWebViewDelegate *)initWithQAbstractWebView:(QIosWebViewPrivate *)webViewPrivate
{
    qIosWebViewPrivate = webViewPrivate;
    return self;
}

- (void)pageDone
{
    Q_EMIT qIosWebViewPrivate->loadProgressChanged(qIosWebViewPrivate->loadProgress());
    Q_EMIT qIosWebViewPrivate->titleChanged(qIosWebViewPrivate->title());
}

- (void)webViewDidStartLoad:(UIWebView *)webView
{
    Q_UNUSED(webView);
    // UIWebViewDelegate gives us per-frame notifications while the QWebView API
    // should provide per-page notifications. Keep track of started frame loads
    // and emit notifications when the final frame completes.
    if (++qIosWebViewPrivate->requestFrameCount == 1) {
        Q_EMIT qIosWebViewPrivate->loadingChanged(QWebViewLoadRequestPrivate(qIosWebViewPrivate->url(),
                                                                             QWebView::LoadStartedStatus,
                                                                             QString()));
    }

    Q_EMIT qIosWebViewPrivate->loadProgressChanged(qIosWebViewPrivate->loadProgress());
}

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    Q_UNUSED(webView);
    if (--qIosWebViewPrivate->requestFrameCount == 0) {
        [self pageDone];
        Q_EMIT qIosWebViewPrivate->loadingChanged(QWebViewLoadRequestPrivate(qIosWebViewPrivate->url(),
                                                                             QWebView::LoadSucceededStatus,
                                                                             QString()));
    }
}

- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
{
    Q_UNUSED(webView);
    if (--qIosWebViewPrivate->requestFrameCount == 0) {
        [self pageDone];
        NSString *errorString = [error localizedFailureReason];
        Q_EMIT qIosWebViewPrivate->loadingChanged(QWebViewLoadRequestPrivate(qIosWebViewPrivate->url(),
                                                                             QWebView::LoadFailedStatus,
                                                                             QString::fromNSString(errorString)));
    }
}
@end

QIosWebViewPrivate::QIosWebViewPrivate(QObject *p)
    : QWebViewPrivate(p)
    , uiWebView(0)
    , m_recognizer(0)
{
    CGRect frame = CGRectMake(0.0, 0.0, 400, 400);
    uiWebView = [[UIWebView alloc] initWithFrame:frame];
    uiWebView.delegate = [[QtWebViewDelegate alloc] initWithQAbstractWebView:this];

    m_recognizer = [[QIOSNativeViewSelectedRecognizer alloc] initWithQWindowControllerItem:this];
    [uiWebView addGestureRecognizer:m_recognizer];
    uiWebView.scalesPageToFit = YES;
}

QIosWebViewPrivate::~QIosWebViewPrivate()
{
    [uiWebView.delegate release];
    uiWebView.delegate = nil; // reset as per UIWebViewDelegate documentation
    [uiWebView release];
    [m_recognizer release];
}

QUrl QIosWebViewPrivate::url() const
{
    NSURL *url = [[uiWebView request] URL];
    return QUrl::fromNSURL(url).toString();
}

void QIosWebViewPrivate::setUrl(const QUrl &url)
{
    requestFrameCount = 0;
    [uiWebView loadRequest:[NSURLRequest requestWithURL:url.toNSURL()]];
}

void QIosWebViewPrivate::loadHtml(const QString &html, const QUrl &baseUrl)
{
    [uiWebView loadHTMLString:html.toNSString() baseURL:baseUrl.toNSURL()];
}

bool QIosWebViewPrivate::canGoBack() const
{
    return uiWebView.canGoBack;
}

bool QIosWebViewPrivate::canGoForward() const
{
    return uiWebView.canGoForward;
}

QString QIosWebViewPrivate::title() const
{
    NSString *title = [uiWebView stringByEvaluatingJavaScriptFromString:@"document.title"];
    return QString::fromNSString(title);
}

int QIosWebViewPrivate::loadProgress() const
{
    // TODO:
    return isLoading() ? 100 : 0;
}

bool QIosWebViewPrivate::isLoading() const
{
    return uiWebView.loading;
}

void QIosWebViewPrivate::setParentView(QObject *view)
{
    m_parentView = view;

    if (!uiWebView)
        return;

    QWindow *w = qobject_cast<QWindow *>(view);
    if (w) {
        UIView *parentView = reinterpret_cast<UIView *>(w->winId());
        [parentView addSubview:uiWebView];
    } else {
        [uiWebView removeFromSuperview];
    }
}

QObject *QIosWebViewPrivate::parentView() const
{
    return m_parentView;
}

void QIosWebViewPrivate::setGeometry(const QRect &geometry)
{
    if (!uiWebView)
        return;

    [uiWebView setFrame:toCGRect(geometry)];
}

void QIosWebViewPrivate::setVisibility(QWindow::Visibility visibility)
{
    Q_UNUSED(visibility);
}

void QIosWebViewPrivate::setVisible(bool visible)
{
    [uiWebView setHidden:!visible];
}

void QIosWebViewPrivate::setFocus(bool focus)
{
    Q_EMIT requestFocus(focus);
}

void QIosWebViewPrivate::goBack()
{
    [uiWebView goBack];
}

void QIosWebViewPrivate::goForward()
{
    [uiWebView goForward];
}

void QIosWebViewPrivate::stop()
{
    [uiWebView stopLoading];
}

void QIosWebViewPrivate::reload()
{
    [uiWebView reload];
}

void QIosWebViewPrivate::runJavaScriptPrivate(const QString &script, int callbackId)
{
    // ### TODO needs more async
    NSString *result = [uiWebView stringByEvaluatingJavaScriptFromString:script.toNSString()];
    if (callbackId != -1)
        Q_EMIT javaScriptResult(callbackId, QString::fromNSString(result));
}

QT_END_NAMESPACE
