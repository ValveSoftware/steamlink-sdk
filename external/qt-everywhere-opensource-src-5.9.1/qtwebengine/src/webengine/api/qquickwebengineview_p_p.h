/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKWEBENGINEVIEW_P_P_H
#define QQUICKWEBENGINEVIEW_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickwebengineview_p.h"
#include "web_contents_adapter_client.h"

#include <QScopedPointer>
#include <QSharedData>
#include <QString>
#include <QtCore/qcompilerdetection.h>
#include <QtGui/qaccessibleobject.h>

namespace QtWebEngineCore {
class WebContentsAdapter;
class UIDelegatesManager;
}

QT_BEGIN_NAMESPACE
class QQuickWebEngineView;
class QQmlComponent;
class QQmlContext;
class QQuickWebEngineContextMenuRequest;
class QQuickWebEngineSettings;
class QQuickWebEngineFaviconProvider;

QQuickWebEngineView::WebAction editorActionForKeyEvent(QKeyEvent* event);

#ifdef ENABLE_QML_TESTSUPPORT_API
class QQuickWebEngineTestSupport;
#endif

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineViewPrivate : public QtWebEngineCore::WebContentsAdapterClient
{
public:
    Q_DECLARE_PUBLIC(QQuickWebEngineView)
    QQuickWebEngineView *q_ptr;
    QQuickWebEngineViewPrivate();
    ~QQuickWebEngineViewPrivate();

    QtWebEngineCore::UIDelegatesManager *ui();

    virtual QtWebEngineCore::RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegate(QtWebEngineCore::RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual QtWebEngineCore::RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegateForPopup(QtWebEngineCore::RenderWidgetHostViewQtDelegateClient *client) Q_DECL_OVERRIDE;
    virtual void titleChanged(const QString&) Q_DECL_OVERRIDE;
    virtual void urlChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void iconChanged(const QUrl&) Q_DECL_OVERRIDE;
    virtual void loadProgressChanged(int progress) Q_DECL_OVERRIDE;
    virtual void didUpdateTargetURL(const QUrl&) Q_DECL_OVERRIDE;
    virtual void selectionChanged() Q_DECL_OVERRIDE { }
    virtual void recentlyAudibleChanged(bool recentlyAudible) Q_DECL_OVERRIDE;
    virtual QRectF viewportRect() const Q_DECL_OVERRIDE;
    virtual qreal dpiScale() const Q_DECL_OVERRIDE;
    virtual QColor backgroundColor() const Q_DECL_OVERRIDE;
    virtual void loadStarted(const QUrl &provisionalUrl, bool isErrorPage = false) Q_DECL_OVERRIDE;
    virtual void loadCommitted() Q_DECL_OVERRIDE;
    virtual void loadVisuallyCommitted() Q_DECL_OVERRIDE;
    virtual void loadFinished(bool success, const QUrl &url, bool isErrorPage = false, int errorCode = 0, const QString &errorDescription = QString()) Q_DECL_OVERRIDE;
    virtual void focusContainer() Q_DECL_OVERRIDE;
    virtual void unhandledKeyEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    virtual void adoptNewWindow(QSharedPointer<QtWebEngineCore::WebContentsAdapter> newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &, const QUrl &targetUrl) Q_DECL_OVERRIDE;
    virtual bool isBeingAdopted() Q_DECL_OVERRIDE;
    virtual void close() Q_DECL_OVERRIDE;
    virtual void windowCloseRejected() Q_DECL_OVERRIDE;
    virtual void requestFullScreenMode(const QUrl &origin, bool fullscreen) Q_DECL_OVERRIDE;
    virtual bool isFullScreenMode() const Q_DECL_OVERRIDE;
    virtual bool contextMenuRequested(const QtWebEngineCore::WebEngineContextMenuData &) Q_DECL_OVERRIDE;
    virtual void navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame) Q_DECL_OVERRIDE;
    virtual void javascriptDialog(QSharedPointer<QtWebEngineCore::JavaScriptDialogController>) Q_DECL_OVERRIDE;
    virtual void runFileChooser(QSharedPointer<QtWebEngineCore::FilePickerController>) Q_DECL_OVERRIDE;
    virtual void showColorDialog(QSharedPointer<QtWebEngineCore::ColorChooserController>) Q_DECL_OVERRIDE;
    virtual void didRunJavaScript(quint64, const QVariant&) Q_DECL_OVERRIDE;
    virtual void didFetchDocumentMarkup(quint64, const QString&) Q_DECL_OVERRIDE { }
    virtual void didFetchDocumentInnerText(quint64, const QString&) Q_DECL_OVERRIDE { }
    virtual void didFindText(quint64, int) Q_DECL_OVERRIDE;
    virtual void didPrintPage(quint64 requestId, const QByteArray &result) Q_DECL_OVERRIDE;
    virtual void didPrintPageToPdf(const QString &filePath, bool success) Q_DECL_OVERRIDE;
    virtual void passOnFocus(bool reverse) Q_DECL_OVERRIDE;
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) Q_DECL_OVERRIDE;
    virtual void authenticationRequired(QSharedPointer<QtWebEngineCore::AuthenticationDialogController>) Q_DECL_OVERRIDE;
    virtual void runMediaAccessPermissionRequest(const QUrl &securityOrigin, MediaRequestFlags requestFlags) Q_DECL_OVERRIDE;
    virtual void runMouseLockPermissionRequest(const QUrl &securityOrigin) Q_DECL_OVERRIDE;
    virtual QObject *accessibilityParentObject() Q_DECL_OVERRIDE;
    virtual QtWebEngineCore::WebEngineSettings *webEngineSettings() const Q_DECL_OVERRIDE;
    virtual void allowCertificateError(const QSharedPointer<CertificateErrorController> &errorController) Q_DECL_OVERRIDE;
    virtual void runGeolocationPermissionRequest(QUrl const&) Q_DECL_OVERRIDE;
    virtual void showValidationMessage(const QRect &anchor, const QString &mainText, const QString &subText) Q_DECL_OVERRIDE;
    virtual void hideValidationMessage() Q_DECL_OVERRIDE;
    virtual void moveValidationMessage(const QRect &anchor) Q_DECL_OVERRIDE;
    virtual void renderProcessTerminated(RenderProcessTerminationStatus terminationStatus,
                                     int exitCode) Q_DECL_OVERRIDE;
    virtual void requestGeometryChange(const QRect &geometry) Q_DECL_OVERRIDE { Q_UNUSED(geometry); }
    virtual void updateScrollPosition(const QPointF &position) Q_DECL_OVERRIDE;
    virtual void updateContentsSize(const QSizeF &size) Q_DECL_OVERRIDE;
    void startDragging(const content::DropData &dropData, Qt::DropActions allowedActions,
                       const QPixmap &pixmap, const QPoint &offset) Q_DECL_OVERRIDE;
    virtual bool isEnabled() const Q_DECL_OVERRIDE;
    virtual void setToolTip(const QString &toolTipText) Q_DECL_OVERRIDE;
    const QObject *holdingQObject() const Q_DECL_OVERRIDE;

    virtual QSharedPointer<QtWebEngineCore::BrowserContextAdapter> browserContextAdapter() Q_DECL_OVERRIDE;
    QtWebEngineCore::WebContentsAdapter *webContentsAdapter() Q_DECL_OVERRIDE;

    void setDevicePixelRatio(qreal);
    void adoptWebContents(QtWebEngineCore::WebContentsAdapter *webContents);
    void setProfile(QQuickWebEngineProfile *profile);
    void ensureContentsAdapter();
    void setFullScreenMode(bool);

    // QQmlListPropertyHelpers
    static void userScripts_append(QQmlListProperty<QQuickWebEngineScript> *p, QQuickWebEngineScript *script);
    static int userScripts_count(QQmlListProperty<QQuickWebEngineScript> *p);
    static QQuickWebEngineScript *userScripts_at(QQmlListProperty<QQuickWebEngineScript> *p, int idx);
    static void userScripts_clear(QQmlListProperty<QQuickWebEngineScript> *p);

    QSharedPointer<QtWebEngineCore::WebContentsAdapter> adapter;
    QScopedPointer<QQuickWebEngineHistory> m_history;
    QQuickWebEngineProfile *m_profile;
    QScopedPointer<QQuickWebEngineSettings> m_settings;
#ifdef ENABLE_QML_TESTSUPPORT_API
    QQuickWebEngineTestSupport *m_testSupport;
#endif
    QQmlComponent *contextMenuExtraItems;
    QtWebEngineCore::WebEngineContextMenuData m_contextMenuData;
    QUrl explicitUrl;
    QUrl iconUrl;
    QQuickWebEngineFaviconProvider *faviconProvider;
    int loadProgress;
    bool m_fullscreenMode;
    bool isLoading;
    bool m_activeFocusOnPress;
    bool m_navigationActionTriggered;
    qreal devicePixelRatio;
    QMap<quint64, QJSValue> m_callbacks;
    QList<QSharedPointer<CertificateErrorController> > m_certificateErrorControllers;
    QQmlWebChannel *m_webChannel;
    uint m_webChannelWorld;

private:
    QScopedPointer<QtWebEngineCore::UIDelegatesManager> m_uIDelegatesManager;
    QList<QQuickWebEngineScript *> m_userScripts;
    qreal m_dpiScale;
    QColor m_backgroundColor;
    qreal m_defaultZoomFactor;
    bool m_ui2Enabled;
};

#ifndef QT_NO_ACCESSIBILITY
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
#endif // QT_NO_ACCESSIBILITY
QT_END_NAMESPACE

#endif // QQUICKWEBENGINEVIEW_P_P_H
