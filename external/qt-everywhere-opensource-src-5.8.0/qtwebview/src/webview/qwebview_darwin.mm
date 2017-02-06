/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qwebview_darwin_p.h"
#include "qwebview_p.h"
#include "qwebviewloadrequest_p.h"
#include "qtwebviewfunctions.h"
#include "qtwebviewfunctions_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>

#include <CoreFoundation/CoreFoundation.h>
#include <WebKit/WebKit.h>

#ifdef Q_OS_IOS
#include "qwebview_ios_p.h"

#include <UIKit/UIKit.h>

#import <UIKit/UIView.h>
#import <UIKit/UIWindow.h>
#import <UIKit/UIViewController.h>
#import <UIKit/UITapGestureRecognizer.h>
#import <UIKit/UIGestureRecognizerSubclass.h>
#endif

#ifdef Q_OS_OSX
#include "qwebview_osx_p.h"
#include "qwebview_webengine_p.h"

#include <AppKit/AppKit.h>

typedef NSView UIView;
#endif

QT_BEGIN_NAMESPACE

inline QSysInfo::MacVersion qt_OS_limit(QSysInfo::MacVersion osxVersion,
                                        QSysInfo::MacVersion iosVersion)
{
#ifdef Q_OS_OSX
    Q_UNUSED(iosVersion)
    return osxVersion;
#else
    Q_UNUSED(osxVersion)
    return iosVersion;
#endif
}

QWebViewPrivate *QWebViewPrivate::create(QWebView *q)
{
#if QT_MACOS_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_10, __IPHONE_8_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_10, QSysInfo::MV_IOS_8_0)
            && QtWebViewPrivate::useNativeWebView())
        return new QDarwinWebViewPrivate(q);
#endif

#if defined(Q_OS_IOS)
    return new QIosWebViewPrivate(q);
#else
#  if defined(Q_OS_MACOS)
    if (QtWebViewPrivate::useNativeWebView())
        return new QOsxWebViewPrivate(q);
#  endif
    return new QWebEngineWebViewPrivate(q);
#endif
}

static inline CGRect toCGRect(const QRectF &rect)
{
    return CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}

QT_END_NAMESPACE
// -------------------------------------------------------------------------

#ifdef Q_OS_IOS
@implementation QIOSNativeViewSelectedRecognizer

- (id)initWithQWindowControllerItem:(QNativeViewController *)item
{
    self = [super initWithTarget:self action:@selector(nativeViewSelected:)];
    if (self) {
        self.cancelsTouchesInView = NO;
        self.delaysTouchesEnded = NO;
        m_item = item;
    }
    return self;
}

- (BOOL)canPreventGestureRecognizer:(UIGestureRecognizer *)other
{
    Q_UNUSED(other);
    return NO;
}

- (BOOL)canBePreventedByGestureRecognizer:(UIGestureRecognizer *)other
{
    Q_UNUSED(other);
    return NO;
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    Q_UNUSED(touches);
    Q_UNUSED(event);
    self.state = UIGestureRecognizerStateRecognized;
}

- (void)nativeViewSelected:(UIGestureRecognizer *)gestureRecognizer
{
    Q_UNUSED(gestureRecognizer);
    m_item->setFocus(true);
}

@end
#endif

// -------------------------------------------------------------------------

#if QT_MACOS_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_10, __IPHONE_8_0)

@interface QtWKWebViewDelegate : NSObject<WKNavigationDelegate> {
    QDarwinWebViewPrivate *qDarwinWebViewPrivate;
}
- (QtWKWebViewDelegate *)initWithQAbstractWebView:(QDarwinWebViewPrivate *)webViewPrivate;
- (void)pageDone;

// protocol:
- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation;
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation;
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation
      withError:(NSError *)error;
@end

@implementation QtWKWebViewDelegate
- (QtWKWebViewDelegate *)initWithQAbstractWebView:(QDarwinWebViewPrivate *)webViewPrivate
{
    if ((self = [super init])) {
        Q_ASSERT(webViewPrivate);
        qDarwinWebViewPrivate = webViewPrivate;
    }
    return self;
}

- (void)pageDone
{
    Q_EMIT qDarwinWebViewPrivate->loadProgressChanged(qDarwinWebViewPrivate->loadProgress());
    Q_EMIT qDarwinWebViewPrivate->titleChanged(qDarwinWebViewPrivate->title());
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    Q_UNUSED(webView);
    Q_UNUSED(navigation);
    // WKNavigationDelegate gives us per-frame notifications while the QWebView API
    // should provide per-page notifications. Keep track of started frame loads
    // and emit notifications when the final frame completes.
    if (++qDarwinWebViewPrivate->requestFrameCount == 1) {
        Q_EMIT qDarwinWebViewPrivate->loadingChanged(
                    QWebViewLoadRequestPrivate(qDarwinWebViewPrivate->url(),
                                               QWebView::LoadStartedStatus,
                                               QString()));
    }

    Q_EMIT qDarwinWebViewPrivate->loadProgressChanged(qDarwinWebViewPrivate->loadProgress());
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    Q_UNUSED(webView);
    Q_UNUSED(navigation);
    if (--qDarwinWebViewPrivate->requestFrameCount == 0) {
        [self pageDone];
        Q_EMIT qDarwinWebViewPrivate->loadingChanged(
                    QWebViewLoadRequestPrivate(qDarwinWebViewPrivate->url(),
                                               QWebView::LoadSucceededStatus,
                                               QString()));
    }
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation
      withError:(NSError *)error
{
    Q_UNUSED(webView);
    Q_UNUSED(navigation);
    if (--qDarwinWebViewPrivate->requestFrameCount == 0) {
        [self pageDone];
        NSString *errorString = [error localizedDescription];
        Q_EMIT qDarwinWebViewPrivate->loadingChanged(
                    QWebViewLoadRequestPrivate(qDarwinWebViewPrivate->url(),
                                               QWebView::LoadFailedStatus,
                                               QString::fromNSString(errorString)));
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change
                       context:(void *)context {
    Q_UNUSED(object);
    Q_UNUSED(change);
    Q_UNUSED(context);
    if ([keyPath isEqualToString:@"estimatedProgress"]) {
        Q_EMIT qDarwinWebViewPrivate->loadProgressChanged(qDarwinWebViewPrivate->loadProgress());
    }
}

@end

QT_BEGIN_NAMESPACE

QDarwinWebViewPrivate::QDarwinWebViewPrivate(QObject *p)
    : QWebViewPrivate(p)
    , wkWebView(nil)
#ifdef Q_OS_IOS
    , m_recognizer(0)
#endif
{
    CGRect frame = CGRectMake(0.0, 0.0, 400, 400);
    wkWebView = [[WKWebView alloc] initWithFrame:frame];
    wkWebView.navigationDelegate = [[QtWKWebViewDelegate alloc] initWithQAbstractWebView:this];
    [wkWebView addObserver:wkWebView.navigationDelegate forKeyPath:@"estimatedProgress"
                   options:NSKeyValueObservingOptions(NSKeyValueObservingOptionNew)
                   context:nil];

#ifdef Q_OS_IOS
    m_recognizer = [[QIOSNativeViewSelectedRecognizer alloc] initWithQWindowControllerItem:this];
    [wkWebView addGestureRecognizer:m_recognizer];
#endif
}

QDarwinWebViewPrivate::~QDarwinWebViewPrivate()
{
    [wkWebView stopLoading];
    [wkWebView removeObserver:wkWebView.navigationDelegate forKeyPath:@"estimatedProgress"
                      context:nil];
    [wkWebView.navigationDelegate release];
    wkWebView.navigationDelegate = nil;
    [wkWebView release];
#ifdef Q_OS_IOS
    [m_recognizer release];
#endif
}

QUrl QDarwinWebViewPrivate::url() const
{
    return QUrl::fromNSURL(wkWebView.URL);
}

void QDarwinWebViewPrivate::setUrl(const QUrl &url)
{
    if (url.isValid()) {
        requestFrameCount = 0;
        [wkWebView loadRequest:[NSURLRequest requestWithURL:url.toNSURL()]];
    }
}

void QDarwinWebViewPrivate::loadHtml(const QString &html, const QUrl &baseUrl)
{
    [wkWebView loadHTMLString:html.toNSString() baseURL:baseUrl.toNSURL()];
}

bool QDarwinWebViewPrivate::canGoBack() const
{
    return wkWebView.canGoBack;
}

bool QDarwinWebViewPrivate::canGoForward() const
{
    return wkWebView.canGoForward;
}

QString QDarwinWebViewPrivate::title() const
{
    return QString::fromNSString(wkWebView.title);
}

int QDarwinWebViewPrivate::loadProgress() const
{
    return int(wkWebView.estimatedProgress * 100);
}

bool QDarwinWebViewPrivate::isLoading() const
{
    return wkWebView.loading;
}

void QDarwinWebViewPrivate::setParentView(QObject *view)
{
    m_parentView = view;

    if (!wkWebView)
        return;

    QWindow *w = qobject_cast<QWindow *>(view);
    if (w) {
        UIView *parentView = reinterpret_cast<UIView *>(w->winId());
        [parentView addSubview:wkWebView];
    } else {
        [wkWebView removeFromSuperview];
    }
}

QObject *QDarwinWebViewPrivate::parentView() const
{
    return m_parentView;
}

void QDarwinWebViewPrivate::setGeometry(const QRect &geometry)
{
    if (!wkWebView)
        return;

    [wkWebView setFrame:toCGRect(geometry)];
}

void QDarwinWebViewPrivate::setVisibility(QWindow::Visibility visibility)
{
    Q_UNUSED(visibility);
}

void QDarwinWebViewPrivate::setVisible(bool visible)
{
    [wkWebView setHidden:!visible];
}

void QDarwinWebViewPrivate::setFocus(bool focus)
{
    Q_EMIT requestFocus(focus);
}

void QDarwinWebViewPrivate::goBack()
{
    [wkWebView goBack];
}

void QDarwinWebViewPrivate::goForward()
{
    [wkWebView goForward];
}

void QDarwinWebViewPrivate::stop()
{
    [wkWebView stopLoading];
}

void QDarwinWebViewPrivate::reload()
{
    [wkWebView reload];
}

QVariant fromNSNumber(const NSNumber *number)
{
    if (!number)
        return QVariant();
    if (strcmp([number objCType], @encode(BOOL)) == 0) {
        return QVariant::fromValue(!![number boolValue]);
    } else if (strcmp([number objCType], @encode(signed char)) == 0) {
        return QVariant::fromValue([number charValue]);
    } else if (strcmp([number objCType], @encode(unsigned char)) == 0) {
        return QVariant::fromValue([number unsignedCharValue]);
    } else if (strcmp([number objCType], @encode(signed short)) == 0) {
        return QVariant::fromValue([number shortValue]);
    } else if (strcmp([number objCType], @encode(unsigned short)) == 0) {
        return QVariant::fromValue([number unsignedShortValue]);
    } else if (strcmp([number objCType], @encode(signed int)) == 0) {
        return QVariant::fromValue([number intValue]);
    } else if (strcmp([number objCType], @encode(unsigned int)) == 0) {
        return QVariant::fromValue([number unsignedIntValue]);
    } else if (strcmp([number objCType], @encode(signed long long)) == 0) {
        return QVariant::fromValue([number longLongValue]);
    } else if (strcmp([number objCType], @encode(unsigned long long)) == 0) {
        return QVariant::fromValue([number unsignedLongLongValue]);
    } else if (strcmp([number objCType], @encode(float)) == 0) {
        return QVariant::fromValue([number floatValue]);
    } else if (strcmp([number objCType], @encode(double)) == 0) {
        return QVariant::fromValue([number doubleValue]);
    }
    return QVariant();
}

QVariant fromJSValue(id result)
{
    if ([result isKindOfClass:[NSString class]])
        return QString::fromNSString(static_cast<NSString *>(result));
    if ([result isKindOfClass:[NSNumber class]])
        return fromNSNumber(static_cast<NSNumber *>(result));
    if ([result isKindOfClass:[NSDate class]])
        return QDateTime::fromNSDate(static_cast<NSDate *>(result));

    // JSValue also supports arrays and dictionaries, but we don't handle that yet
    return QVariant();
}

void QDarwinWebViewPrivate::runJavaScriptPrivate(const QString &script, int callbackId)
{
    [wkWebView evaluateJavaScript:script.toNSString() completionHandler:^(id result, NSError *) {
        if (callbackId != -1)
            Q_EMIT javaScriptResult(callbackId, fromJSValue(result));
    }];
}
#endif

QT_END_NAMESPACE
