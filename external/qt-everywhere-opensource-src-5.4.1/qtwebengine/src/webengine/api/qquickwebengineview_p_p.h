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

#ifndef QQUICKWEBENGINEVIEW_P_P_H
#define QQUICKWEBENGINEVIEW_P_P_H

#include "qquickwebengineview_p.h"
#include "web_contents_adapter_client.h"

#include <QScopedPointer>
#include <QSharedData>
#include <QString>
#include <QtCore/qcompilerdetection.h>
#include <QtGui/qaccessibleobject.h>

class WebContentsAdapter;
class UIDelegatesManager;

QT_BEGIN_NAMESPACE
class QQuickWebEngineHistory;
class QQuickWebEngineNewViewRequest;
class QQuickWebEngineView;
class QQmlComponent;
class QQmlContext;
class QQuickWebEngineSettings;

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewport : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
public:
    QQuickWebEngineViewport(QQuickWebEngineViewPrivate *viewPrivate);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal);

Q_SIGNALS:
    void devicePixelRatioChanged();

private:
    QQuickWebEngineViewPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QQuickWebEngineView)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewExperimental : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWebEngineViewport *viewport READ viewport)
    Q_PROPERTY(QQmlComponent *extraContextMenuEntriesComponent READ extraContextMenuEntriesComponent WRITE setExtraContextMenuEntriesComponent NOTIFY extraContextMenuEntriesComponentChanged)
    Q_PROPERTY(bool inspectable READ inspectable WRITE setInspectable)
    Q_PROPERTY(bool isFullScreen READ isFullScreen WRITE setIsFullScreen NOTIFY isFullScreenChanged)
    Q_PROPERTY(QQuickWebEngineHistory *navigationHistory READ navigationHistory CONSTANT FINAL)
    Q_PROPERTY(QQuickWebEngineSettings *settings READ settings)
    Q_ENUMS(Feature)
    Q_FLAGS(FindFlags)

public:
    enum Feature {
        MediaAudioDevices,
        MediaVideoDevices,
        MediaAudioVideoDevices
    };

    enum FindFlag {
        FindBackward = 1,
        FindCaseSensitively = 2,
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag)

    bool inspectable() const;
    void setInspectable(bool);
    void setIsFullScreen(bool fullscreen);
    bool isFullScreen() const;
    QQuickWebEngineViewport *viewport() const;
    void setExtraContextMenuEntriesComponent(QQmlComponent *);
    QQmlComponent *extraContextMenuEntriesComponent() const;
    QQuickWebEngineHistory *navigationHistory() const;
    QQuickWebEngineSettings *settings() const;

public Q_SLOTS:
    void goBackTo(int index);
    void goForwardTo(int index);
    void findText(const QString&, FindFlags, const QJSValue & = QJSValue());
    void grantFeaturePermission(const QUrl &securityOrigin, Feature, bool granted);

Q_SIGNALS:
    void newViewRequested(QQuickWebEngineNewViewRequest *request);
    void fullScreenRequested(bool fullScreen);
    void isFullScreenChanged();
    void extraContextMenuEntriesComponentChanged();
    void featurePermissionRequested(const QUrl &securityOrigin, Feature feature);
    void loadVisuallyCommitted();

private:
    QQuickWebEngineViewExperimental(QQuickWebEngineViewPrivate* viewPrivate);
    QQuickWebEngineView *q_ptr;
    QQuickWebEngineViewPrivate *d_ptr;

    Q_DECLARE_PRIVATE(QQuickWebEngineView)
    Q_DECLARE_PUBLIC(QQuickWebEngineView)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewPrivate : public WebContentsAdapterClient
{
public:
    Q_DECLARE_PUBLIC(QQuickWebEngineView)
    QQuickWebEngineView *q_ptr;
    QQuickWebEngineViewPrivate();
    ~QQuickWebEngineViewPrivate();

    QQuickWebEngineViewExperimental *experimental() const;
    QQuickWebEngineViewport *viewport() const;
    UIDelegatesManager *ui();

    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegateForPopup(RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual void titleChanged(const QString&) Q_DECL_OVERRIDE;
    virtual void urlChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void iconChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void loadProgressChanged(int progress) Q_DECL_OVERRIDE;
    virtual void didUpdateTargetURL(const QUrl&) Q_DECL_OVERRIDE;
    virtual void selectionChanged() Q_DECL_OVERRIDE { }
    virtual QRectF viewportRect() const Q_DECL_OVERRIDE;
    virtual qreal dpiScale() const Q_DECL_OVERRIDE;
    virtual void loadStarted(const QUrl &provisionalUrl) Q_DECL_OVERRIDE;
    virtual void loadCommitted() Q_DECL_OVERRIDE;
    virtual void loadVisuallyCommitted() Q_DECL_OVERRIDE;
    virtual void loadFinished(bool success, const QUrl &url, int errorCode = 0, const QString &errorDescription = QString()) Q_DECL_OVERRIDE;
    virtual void focusContainer() Q_DECL_OVERRIDE;
    virtual void adoptNewWindow(WebContentsAdapter *newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &) Q_DECL_OVERRIDE;
    virtual void close() Q_DECL_OVERRIDE;
    virtual void requestFullScreen(bool) Q_DECL_OVERRIDE;
    virtual bool isFullScreen() const Q_DECL_OVERRIDE;
    virtual bool contextMenuRequested(const WebEngineContextMenuData &) Q_DECL_OVERRIDE;
    virtual void navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame) Q_DECL_OVERRIDE;
    virtual void javascriptDialog(QSharedPointer<JavaScriptDialogController>) Q_DECL_OVERRIDE;
    virtual void runFileChooser(FileChooserMode, const QString &defaultFileName, const QStringList &acceptedMimeTypes) Q_DECL_OVERRIDE;
    virtual void didRunJavaScript(quint64, const QVariant&) Q_DECL_OVERRIDE;
    virtual void didFetchDocumentMarkup(quint64, const QString&) Q_DECL_OVERRIDE { }
    virtual void didFetchDocumentInnerText(quint64, const QString&) Q_DECL_OVERRIDE { }
    virtual void didFindText(quint64, int) Q_DECL_OVERRIDE;
    virtual void passOnFocus(bool reverse) Q_DECL_OVERRIDE;
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) Q_DECL_OVERRIDE;
    virtual void authenticationRequired(const QUrl&, const QString&, bool, const QString&, QString*, QString*) Q_DECL_OVERRIDE { }
    virtual void runMediaAccessPermissionRequest(const QUrl &securityOrigin, MediaRequestFlags requestFlags) Q_DECL_OVERRIDE;
    virtual QObject *accessibilityParentObject() Q_DECL_OVERRIDE;
    virtual WebEngineSettings *webEngineSettings() const Q_DECL_OVERRIDE;
    virtual void allowCertificateError(const QExplicitlySharedDataPointer<CertificateErrorController> &errorController);

    void setDevicePixelRatio(qreal);
    void adoptWebContents(WebContentsAdapter *webContents);

    QExplicitlySharedDataPointer<WebContentsAdapter> adapter;
    QScopedPointer<QQuickWebEngineViewExperimental> e;
    QScopedPointer<QQuickWebEngineViewport> v;
    QScopedPointer<QQuickWebEngineHistory> m_history;
    QScopedPointer<QQuickWebEngineSettings> m_settings;
    QQmlComponent *contextMenuExtraItems;
    QUrl explicitUrl;
    QUrl icon;
    int loadProgress;
    bool inspectable;
    bool m_isFullScreen;
    bool isLoading;
    qreal devicePixelRatio;
    QMap<quint64, QJSValue> m_callbacks;

private:
    QScopedPointer<UIDelegatesManager> m_uIDelegatesManager;
    qreal m_dpiScale;
};

class QQuickWebEngineViewAccessible : public QAccessibleObject
{
public:
    QQuickWebEngineViewAccessible(QQuickWebEngineView *o);
    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    int childCount() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;
    int indexOfChild(const QAccessibleInterface*) const Q_DECL_OVERRIDE;
    QString text(QAccessible::Text) const Q_DECL_OVERRIDE;
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;

private:
    QQuickWebEngineView *engineView() const { return static_cast<QQuickWebEngineView*>(object()); }
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineViewExperimental)
QML_DECLARE_TYPE(QQuickWebEngineViewport)

#endif // QQUICKWEBENGINEVIEW_P_P_H
