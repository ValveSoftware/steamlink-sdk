/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWEBENGINEPAGE_P_H
#define QWEBENGINEPAGE_P_H

#include "qwebenginepage.h"

#include "web_contents_adapter_client.h"
#include <QtCore/qcompilerdetection.h>
#include <QSharedData>

class RenderWidgetHostViewQtDelegate;
class WebContentsAdapter;

QT_BEGIN_NAMESPACE
class QWebEngineHistory;
class QWebEnginePage;
class QWebEngineSettings;
class QWebEngineView;

class CallbackDirectory {
public:
    typedef QtWebEnginePrivate::QWebEngineCallbackPrivateBase<const QVariant&> VariantCallback;
    typedef QtWebEnginePrivate::QWebEngineCallbackPrivateBase<const QString&> StringCallback;
    typedef QtWebEnginePrivate::QWebEngineCallbackPrivateBase<bool> BoolCallback;

    ~CallbackDirectory();
    void registerCallback(quint64 requestId, const QExplicitlySharedDataPointer<VariantCallback> &callback);
    void registerCallback(quint64 requestId, const QExplicitlySharedDataPointer<StringCallback> &callback);
    void registerCallback(quint64 requestId, const QExplicitlySharedDataPointer<BoolCallback> &callback);
    void invoke(quint64 requestId, const QVariant &result);
    void invoke(quint64 requestId, const QString &result);
    void invoke(quint64 requestId, bool result);

private:
    struct CallbackSharedDataPointer {
        enum {
            None,
            Variant,
            String,
            Bool
        } type;
        union {
            VariantCallback *variantCallback;
            StringCallback *stringCallback;
            BoolCallback *boolCallback;
        };
        CallbackSharedDataPointer() : type(None) { }
        CallbackSharedDataPointer(VariantCallback *callback) : type(Variant), variantCallback(callback) { callback->ref.ref(); }
        CallbackSharedDataPointer(StringCallback *callback) : type(String), stringCallback(callback) { callback->ref.ref(); }
        CallbackSharedDataPointer(BoolCallback *callback) : type(Bool), boolCallback(callback) { callback->ref.ref(); }
        CallbackSharedDataPointer(const CallbackSharedDataPointer &other) : type(other.type), variantCallback(other.variantCallback) { doRef(); }
        ~CallbackSharedDataPointer() { doDeref(); }
        operator bool () const { return type != None; }

    private:
        void doRef();
        void doDeref();
    };

    QHash<quint64, CallbackSharedDataPointer> m_callbackMap;
};

class QWebEnginePagePrivate : public WebContentsAdapterClient
{
public:
    Q_DECLARE_PUBLIC(QWebEnginePage)
    QWebEnginePage *q_ptr;

    QWebEnginePagePrivate();
    ~QWebEnginePagePrivate();

    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegateForPopup(RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE { return CreateRenderWidgetHostViewQtDelegate(client); }
    virtual void titleChanged(const QString&) Q_DECL_OVERRIDE;
    virtual void urlChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void iconChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void loadProgressChanged(int progress) Q_DECL_OVERRIDE;
    virtual void didUpdateTargetURL(const QUrl&) Q_DECL_OVERRIDE;
    virtual void selectionChanged() Q_DECL_OVERRIDE;
    virtual QRectF viewportRect() const Q_DECL_OVERRIDE;
    virtual qreal dpiScale() const Q_DECL_OVERRIDE;
    virtual void loadStarted(const QUrl &provisionalUrl) Q_DECL_OVERRIDE;
    virtual void loadCommitted() Q_DECL_OVERRIDE;
    virtual void loadVisuallyCommitted() Q_DECL_OVERRIDE { }
    virtual void loadFinished(bool success, const QUrl &url, int errorCode = 0, const QString &errorDescription = QString()) Q_DECL_OVERRIDE;
    virtual void focusContainer() Q_DECL_OVERRIDE;
    virtual void adoptNewWindow(WebContentsAdapter *newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &initialGeometry) Q_DECL_OVERRIDE;
    virtual void close() Q_DECL_OVERRIDE;
    virtual bool contextMenuRequested(const WebEngineContextMenuData &data) Q_DECL_OVERRIDE;
    virtual void navigationRequested(int, const QUrl &, int &, bool) Q_DECL_OVERRIDE { }
    virtual void requestFullScreen(bool) Q_DECL_OVERRIDE { }
    virtual bool isFullScreen() const Q_DECL_OVERRIDE { return false; }
    virtual void javascriptDialog(QSharedPointer<JavaScriptDialogController>) Q_DECL_OVERRIDE;
    virtual void runFileChooser(FileChooserMode, const QString &defaultFileName, const QStringList &acceptedMimeTypes) Q_DECL_OVERRIDE;
    virtual void didRunJavaScript(quint64 requestId, const QVariant& result) Q_DECL_OVERRIDE;
    virtual void didFetchDocumentMarkup(quint64 requestId, const QString& result) Q_DECL_OVERRIDE;
    virtual void didFetchDocumentInnerText(quint64 requestId, const QString& result) Q_DECL_OVERRIDE;
    virtual void didFindText(quint64 requestId, int matchCount) Q_DECL_OVERRIDE;
    virtual void passOnFocus(bool reverse) Q_DECL_OVERRIDE;
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) Q_DECL_OVERRIDE;
    virtual void authenticationRequired(const QUrl &requestUrl, const QString &realm, bool isProxy, const QString &challengingHost, QString *outUser, QString *outPassword) Q_DECL_OVERRIDE;
    virtual void runMediaAccessPermissionRequest(const QUrl &securityOrigin, MediaRequestFlags requestFlags) Q_DECL_OVERRIDE;
    virtual QObject *accessibilityParentObject() Q_DECL_OVERRIDE;
    virtual WebEngineSettings *webEngineSettings() const Q_DECL_OVERRIDE;
    virtual void allowCertificateError(const QExplicitlySharedDataPointer<CertificateErrorController> &controller) Q_DECL_OVERRIDE;

    void updateAction(QWebEnginePage::WebAction) const;
    void updateNavigationActions();
    void _q_webActionTriggered(bool checked);

    WebContentsAdapter *webContents() { return adapter.data(); }
    void recreateFromSerializedHistory(QDataStream &input);

    QExplicitlySharedDataPointer<WebContentsAdapter> adapter;
    QWebEngineHistory *history;
    QWebEngineSettings *settings;
    QWebEngineView *view;
    QSize viewportSize;
    QUrl explicitUrl;
    WebEngineContextMenuData m_menuData;
    bool isLoading;

    mutable CallbackDirectory m_callbacks;
    mutable QAction *actions[QWebEnginePage::WebActionCount];
};

QT_END_NAMESPACE

#endif // QWEBENGINEPAGE_P_H
