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

#include "qquickwebengineview_p.h"
#include "qquickwebengineview_p_p.h"

#include "authentication_dialog_controller.h"
#include "browser_context_adapter.h"
#include "certificate_error_controller.h"
#include "file_picker_controller.h"
#include "javascript_dialog_controller.h"
#include "qquickwebenginehistory_p.h"
#include "qquickwebenginecertificateerror_p.h"
#include "qquickwebenginecontextmenurequest_p.h"
#include "qquickwebenginedialogrequests_p.h"
#include "qquickwebenginefaviconprovider_p_p.h"
#include "qquickwebengineloadrequest_p.h"
#include "qquickwebenginenavigationrequest_p.h"
#include "qquickwebenginenewviewrequest_p.h"
#include "qquickwebengineprofile_p.h"
#include "qquickwebenginesettings_p.h"
#include "qquickwebenginescript_p_p.h"

#ifdef ENABLE_QML_TESTSUPPORT_API
#include "qquickwebenginetestsupport_p.h"
#endif

#include "render_widget_host_view_qt_delegate_quick.h"
#include "render_widget_host_view_qt_delegate_quickwindow.h"
#include "renderer_host/user_resource_controller_host.h"
#include "ui_delegates_manager.h"
#include "web_contents_adapter.h"
#include "web_engine_error.h"
#include "web_engine_settings.h"
#include "web_engine_visited_links_manager.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMarginsF>
#include <QMimeData>
#include <QPageLayout>
#include <QPageSize>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQmlWebChannel>
#include <QQuickWebEngineProfile>
#include <QScreen>
#include <QUrl>
#include <QTimer>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#ifndef QT_NO_ACCESSIBILITY
#include <private/qquickaccessibleattached_p.h>
#endif // QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE
using namespace QtWebEngineCore;

QQuickWebEngineView::WebAction editorActionForKeyEvent(QKeyEvent* event)
{
    static struct {
        QKeySequence::StandardKey standardKey;
        QQuickWebEngineView::WebAction action;
    } editorActions[] = {
        { QKeySequence::Cut, QQuickWebEngineView::Cut },
        { QKeySequence::Copy, QQuickWebEngineView::Copy },
        { QKeySequence::Paste, QQuickWebEngineView::Paste },
        { QKeySequence::Undo, QQuickWebEngineView::Undo },
        { QKeySequence::Redo, QQuickWebEngineView::Redo },
        { QKeySequence::SelectAll, QQuickWebEngineView::SelectAll },
        { QKeySequence::UnknownKey, QQuickWebEngineView::NoWebAction }
    };
    for (int i = 0; editorActions[i].standardKey != QKeySequence::UnknownKey; ++i)
        if (event == editorActions[i].standardKey)
            return editorActions[i].action;

    return QQuickWebEngineView::NoWebAction;
}

#ifndef QT_NO_ACCESSIBILITY
static QAccessibleInterface *webAccessibleFactory(const QString &, QObject *object)
{
    if (QQuickWebEngineView *v = qobject_cast<QQuickWebEngineView*>(object))
        return new QQuickWebEngineViewAccessible(v);
    return 0;
}
#endif // QT_NO_ACCESSIBILITY

QQuickWebEngineViewPrivate::QQuickWebEngineViewPrivate()
    : adapter(0)
    , m_history(new QQuickWebEngineHistory(this))
    , m_profile(QQuickWebEngineProfile::defaultProfile())
    , m_settings(new QQuickWebEngineSettings(m_profile->settings()))
#ifdef ENABLE_QML_TESTSUPPORT_API
    , m_testSupport(0)
#endif
    , contextMenuExtraItems(0)
    , faviconProvider(0)
    , loadProgress(0)
    , m_fullscreenMode(false)
    , isLoading(false)
    , m_activeFocusOnPress(true)
    , devicePixelRatio(QGuiApplication::primaryScreen()->devicePixelRatio())
    , m_webChannel(0)
    , m_webChannelWorld(0)
    , m_dpiScale(1.0)
    , m_backgroundColor(Qt::white)
    , m_defaultZoomFactor(1.0)
    , m_ui2Enabled(false)
{
    QString platform = qApp->platformName().toLower();
    if (platform == QLatin1Literal("eglfs"))
        m_ui2Enabled = true;

    const QByteArray dialogSet = qgetenv("QTWEBENGINE_DIALOG_SET");

    if (!dialogSet.isEmpty()) {
        if (dialogSet == QByteArrayLiteral("QtQuickControls2")) {
            m_ui2Enabled = true;
        } else if (dialogSet == QByteArrayLiteral("QtQuickControls1")
                   && m_ui2Enabled) {
            m_ui2Enabled = false;
            qWarning("QTWEBENGINE_DIALOG_SET=QtQuickControls1 forces use of Qt Quick Controls 1 "
                     "on an eglfs backend. This can crash your application!");
        } else {
            qWarning("Ignoring QTWEBENGINE_DIALOG_SET environment variable set to %s. Accepted "
                     "values are \"QtQuickControls1\" and \"QtQuickControls2\"", dialogSet.data());
        }
    }

#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(&webAccessibleFactory);
#endif // QT_NO_ACCESSIBILITY
}

QQuickWebEngineViewPrivate::~QQuickWebEngineViewPrivate()
{
}

UIDelegatesManager *QQuickWebEngineViewPrivate::ui()
{
    Q_Q(QQuickWebEngineView);
    if (m_uIDelegatesManager.isNull())
        m_uIDelegatesManager.reset(m_ui2Enabled ? new UI2DelegatesManager(q) : new UIDelegatesManager(q));
    return m_uIDelegatesManager.data();
}

RenderWidgetHostViewQtDelegate *QQuickWebEngineViewPrivate::CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client)
{
    return new RenderWidgetHostViewQtDelegateQuick(client, /*isPopup = */ false);
}

RenderWidgetHostViewQtDelegate *QQuickWebEngineViewPrivate::CreateRenderWidgetHostViewQtDelegateForPopup(RenderWidgetHostViewQtDelegateClient *client)
{
    Q_Q(QQuickWebEngineView);
    const bool hasWindowCapability = QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::MultipleWindows);
    RenderWidgetHostViewQtDelegateQuick *quickDelegate = new RenderWidgetHostViewQtDelegateQuick(client, /*isPopup = */ true);
    if (hasWindowCapability) {
        RenderWidgetHostViewQtDelegateQuickWindow *wrapperWindow = new RenderWidgetHostViewQtDelegateQuickWindow(quickDelegate);
        quickDelegate->setParentItem(wrapperWindow->contentItem());
        return wrapperWindow;
    }
    quickDelegate->setParentItem(q);
    return quickDelegate;
}

bool QQuickWebEngineViewPrivate::contextMenuRequested(const WebEngineContextMenuData &data)
{
    Q_Q(QQuickWebEngineView);

    m_contextMenuData = data;

    QQuickWebEngineContextMenuRequest *request = new QQuickWebEngineContextMenuRequest(data);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->contextMenuRequested(request);

    if (request->isAccepted())
        return true;

    // Assign the WebEngineView as the parent of the menu, so mouse events are properly propagated
    // on OSX.
    QObject *menu = ui()->addMenu(q, QString(), data.position());
    if (!menu)
        return false;

    // Populate our menu
    MenuItemHandler *item = 0;
    if (data.isEditable() && !data.spellCheckerSuggestions().isEmpty()) {
        const QPointer<QQuickWebEngineView> qRef(q);
        for (int i=0; i < data.spellCheckerSuggestions().count() && i < 4; i++) {
            item = new MenuItemHandler(menu);
            QString replacement = data.spellCheckerSuggestions().at(i);
            QObject::connect(item, &MenuItemHandler::triggered, [qRef, replacement] { qRef->replaceMisspelledWord(replacement); });
            ui()->addMenuItem(item, replacement);
        }
        ui()->addMenuSeparator(menu);
    }
    if (!data.linkText().isEmpty() && data.linkUrl().isValid()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::OpenLinkInThisWindow); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Follow Link"));
    }

    if (data.selectedText().isEmpty()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, q, &QQuickWebEngineView::goBack);
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Back"), QStringLiteral("go-previous"), q->canGoBack());

        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, q, &QQuickWebEngineView::goForward);
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Forward"), QStringLiteral("go-next"), q->canGoForward());

        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, q, &QQuickWebEngineView::reload);
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Reload"), QStringLiteral("view-refresh"));

        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ViewSource); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("View Page Source"), QStringLiteral("view-source"), adapter->canViewSource());
    } else {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::Copy); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy"));
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::Unselect); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Unselect"));
    }

    if (!data.linkText().isEmpty() && data.linkUrl().isValid()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyLinkToClipboard); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Link URL"));
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::DownloadLinkToDisk); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Save Link"));
    }
    if (data.mediaUrl().isValid()) {
        switch (data.mediaType()) {
        case WebEngineContextMenuData::MediaTypeImage:
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyImageUrlToClipboard); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Image URL"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyImageToClipboard); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Image"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::DownloadImageToDisk); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Save Image"));
            break;
        case WebEngineContextMenuData::MediaTypeCanvas:
            Q_UNREACHABLE();    // mediaUrl is invalid for canvases
            break;
        case WebEngineContextMenuData::MediaTypeAudio:
        case WebEngineContextMenuData::MediaTypeVideo:
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyMediaUrlToClipboard); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Media URL"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::DownloadMediaToDisk); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Save Media"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaPlayPause); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Play/Pause"));
            item = new MenuItemHandler(menu);
            QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaLoop); });
            ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Looping"));
            if (data.mediaFlags() & WebEngineContextMenuData::MediaHasAudio) {
                item = new MenuItemHandler(menu);
                QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaMute); });
                ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Mute"));
            }
            if (data.mediaFlags() & WebEngineContextMenuData::MediaCanToggleControls) {
                item = new MenuItemHandler(menu);
                QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ToggleMediaControls); });
                ui()->addMenuItem(item, QQuickWebEngineView::tr("Toggle Media Controls"));
            }
            break;
        default:
            break;
        }
    } else if (data.mediaType() == WebEngineContextMenuData::MediaTypeCanvas) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::CopyImageToClipboard); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Copy Image"));
    }
    if (adapter->hasInspector()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::InspectElement); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Inspect Element"));
    }
    if (isFullScreenMode()) {
        item = new MenuItemHandler(menu);
        QObject::connect(item, &MenuItemHandler::triggered, [q] { q->triggerWebAction(QQuickWebEngineView::ExitFullScreen); });
        ui()->addMenuItem(item, QQuickWebEngineView::tr("Exit Full Screen Mode"));
    }

    // FIXME: expose the context menu data as an attached property to make this more useful
    if (contextMenuExtraItems) {
        ui()->addMenuSeparator(menu);
        if (QObject* menuExtras = contextMenuExtraItems->create(qmlContext(q))) {
            menuExtras->setParent(menu);
            QQmlListReference entries(menu, defaultPropertyName(menu), qmlEngine(q));
            if (entries.isValid())
                entries.append(menuExtras);
        }
    }

    // Now fire the popup() method on the top level menu
    ui()->showMenu(menu);
    return true;
}

void QQuickWebEngineViewPrivate::navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineNavigationRequest navigationRequest(url, static_cast<QQuickWebEngineView::NavigationType>(navigationType), isMainFrame);
    Q_EMIT q->navigationRequested(&navigationRequest);

    navigationRequestAction = navigationRequest.action();
}

void QQuickWebEngineViewPrivate::javascriptDialog(QSharedPointer<JavaScriptDialogController> dialog)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineJavaScriptDialogRequest *request = new QQuickWebEngineJavaScriptDialogRequest(dialog);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->javaScriptDialogRequested(request);
    if (!request->isAccepted())
        ui()->showDialog(dialog);
}

void QQuickWebEngineViewPrivate::allowCertificateError(const QSharedPointer<CertificateErrorController> &errorController)
{
    Q_Q(QQuickWebEngineView);

    QQuickWebEngineCertificateError *quickController = new QQuickWebEngineCertificateError(errorController);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(quickController);
    Q_EMIT q->certificateError(quickController);
    if (!quickController->deferred() && !quickController->answered())
        quickController->rejectCertificate();
    else
        m_certificateErrorControllers.append(errorController);
}

void QQuickWebEngineViewPrivate::runGeolocationPermissionRequest(const QUrl &url)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->featurePermissionRequested(url, QQuickWebEngineView::Geolocation);
}

void QQuickWebEngineViewPrivate::showColorDialog(QSharedPointer<ColorChooserController> controller)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineColorDialogRequest *request = new QQuickWebEngineColorDialogRequest(controller);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->colorDialogRequested(request);
    if (!request->isAccepted())
        ui()->showColorDialog(controller);
}

void QQuickWebEngineViewPrivate::runFileChooser(QSharedPointer<FilePickerController> controller)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineFileDialogRequest *request = new QQuickWebEngineFileDialogRequest(controller);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->fileDialogRequested(request);
    if (!request->isAccepted())
        ui()->showFilePicker(controller);
}

void QQuickWebEngineViewPrivate::passOnFocus(bool reverse)
{
    Q_Q(QQuickWebEngineView);
    // The child delegate currently has focus, find the next one from there and give it focus.
    QQuickItem *next = q->scopedFocusItem()->nextItemInFocusChain(!reverse);
    next->forceActiveFocus(reverse ? Qt::BacktabFocusReason : Qt::TabFocusReason);
}

void QQuickWebEngineViewPrivate::titleChanged(const QString &title)
{
    Q_Q(QQuickWebEngineView);
    Q_UNUSED(title);
    Q_EMIT q->titleChanged();
}

void QQuickWebEngineViewPrivate::urlChanged(const QUrl &url)
{
    Q_Q(QQuickWebEngineView);
    Q_UNUSED(url);
    explicitUrl = QUrl();
    Q_EMIT q->urlChanged();
}

void QQuickWebEngineViewPrivate::iconChanged(const QUrl &url)
{
    Q_Q(QQuickWebEngineView);

    if (iconUrl == QQuickWebEngineFaviconProvider::faviconProviderUrl(url))
        return;

    if (!faviconProvider) {
        QQmlEngine *engine = qmlEngine(q);
        Q_ASSERT(engine);
        faviconProvider = static_cast<QQuickWebEngineFaviconProvider *>(
                    engine->imageProvider(QQuickWebEngineFaviconProvider::identifier()));
        Q_ASSERT(faviconProvider);
    }

    iconUrl = faviconProvider->attach(q, url);
    m_history->reset();
    Q_EMIT q->iconChanged();
}

void QQuickWebEngineViewPrivate::loadProgressChanged(int progress)
{
    Q_Q(QQuickWebEngineView);
    loadProgress = progress;
    Q_EMIT q->loadProgressChanged();
}

void QQuickWebEngineViewPrivate::didUpdateTargetURL(const QUrl &hoveredUrl)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->linkHovered(hoveredUrl);
}

void QQuickWebEngineViewPrivate::recentlyAudibleChanged(bool recentlyAudible)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->recentlyAudibleChanged(recentlyAudible);
}

QRectF QQuickWebEngineViewPrivate::viewportRect() const
{
    Q_Q(const QQuickWebEngineView);
    return QRectF(q->x(), q->y(), q->width(), q->height());
}

qreal QQuickWebEngineViewPrivate::dpiScale() const
{
    return m_dpiScale;
}

QColor QQuickWebEngineViewPrivate::backgroundColor() const
{
    return m_backgroundColor;
}

void QQuickWebEngineViewPrivate::loadStarted(const QUrl &provisionalUrl, bool isErrorPage)
{
    Q_Q(QQuickWebEngineView);
    if (isErrorPage) {
#ifdef ENABLE_QML_TESTSUPPORT_API
        if (m_testSupport)
            m_testSupport->errorPage()->loadStarted(provisionalUrl);
#endif
        return;
    }

    isLoading = true;
    m_history->reset();
    m_certificateErrorControllers.clear();
    QQuickWebEngineLoadRequest loadRequest(provisionalUrl, QQuickWebEngineView::LoadStartedStatus);
    Q_EMIT q->loadingChanged(&loadRequest);
}

void QQuickWebEngineViewPrivate::loadCommitted()
{
    m_history->reset();
}

void QQuickWebEngineViewPrivate::loadVisuallyCommitted()
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    if (m_testSupport)
        Q_EMIT m_testSupport->loadVisuallyCommitted();
#endif
}

Q_STATIC_ASSERT(static_cast<int>(WebEngineError::NoErrorDomain) == static_cast<int>(QQuickWebEngineView::NoErrorDomain));
Q_STATIC_ASSERT(static_cast<int>(WebEngineError::CertificateErrorDomain) == static_cast<int>(QQuickWebEngineView::CertificateErrorDomain));
Q_STATIC_ASSERT(static_cast<int>(WebEngineError::DnsErrorDomain) == static_cast<int>(QQuickWebEngineView::DnsErrorDomain));

void QQuickWebEngineViewPrivate::loadFinished(bool success, const QUrl &url, bool isErrorPage, int errorCode, const QString &errorDescription)
{
    Q_Q(QQuickWebEngineView);

    if (isErrorPage) {
#ifdef ENABLE_QML_TESTSUPPORT_API
        if (m_testSupport)
            m_testSupport->errorPage()->loadFinished(success, url);
#endif
        return;
    }

    isLoading = false;
    m_history->reset();
    if (errorCode == WebEngineError::UserAbortedError) {
        QQuickWebEngineLoadRequest loadRequest(url, QQuickWebEngineView::LoadStoppedStatus);
        Q_EMIT q->loadingChanged(&loadRequest);
        return;
    }
    if (success) {
        explicitUrl = QUrl();
        QQuickWebEngineLoadRequest loadRequest(url, QQuickWebEngineView::LoadSucceededStatus);
        Q_EMIT q->loadingChanged(&loadRequest);
        return;
    }

    Q_ASSERT(errorCode);
    QQuickWebEngineLoadRequest loadRequest(
        url,
        QQuickWebEngineView::LoadFailedStatus,
        errorDescription,
        errorCode,
        static_cast<QQuickWebEngineView::ErrorDomain>(WebEngineError::toQtErrorDomain(errorCode)));
    Q_EMIT q->loadingChanged(&loadRequest);
    return;
}

void QQuickWebEngineViewPrivate::focusContainer()
{
    Q_Q(QQuickWebEngineView);
    q->forceActiveFocus();
}

void QQuickWebEngineViewPrivate::unhandledKeyEvent(QKeyEvent *event)
{
    Q_Q(QQuickWebEngineView);
#ifdef Q_OS_OSX
    if (event->type() == QEvent::KeyPress) {
        QQuickWebEngineView::WebAction action = editorActionForKeyEvent(event);
        if (action != QQuickWebEngineView::NoWebAction) {
            // Try triggering a registered short-cut
            if (QGuiApplicationPrivate::instance()->shortcutMap.tryShortcut(event))
                return;
            q->triggerWebAction(action);
            return;
        }
    }
#endif
    if (q->parentItem())
        q->window()->sendEvent(q->parentItem(), event);
}

void QQuickWebEngineViewPrivate::adoptNewWindow(QSharedPointer<WebContentsAdapter> newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineNewViewRequest request;
    // This increases the ref-count of newWebContents and will tell Chromium
    // to start loading it and possibly return it to its parent page window.open().
    request.m_adapter = newWebContents;
    request.m_isUserInitiated = userGesture;

    switch (disposition) {
    case WebContentsAdapterClient::NewForegroundTabDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInTab;
        break;
    case WebContentsAdapterClient::NewBackgroundTabDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInBackgroundTab;
        break;
    case WebContentsAdapterClient::NewPopupDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInDialog;
        break;
    case WebContentsAdapterClient::NewWindowDisposition:
        request.m_destination = QQuickWebEngineView::NewViewInWindow;
        break;
    default:
        Q_UNREACHABLE();
    }

    Q_EMIT q->newViewRequested(&request);
}

bool QQuickWebEngineViewPrivate::isBeingAdopted()
{
    return false;
}

void QQuickWebEngineViewPrivate::close()
{
    Q_Q(QQuickWebEngineView);
    emit q->windowCloseRequested();
}

void QQuickWebEngineViewPrivate::windowCloseRejected()
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    if (m_testSupport)
        Q_EMIT m_testSupport->windowCloseRejected();
#endif
}

void QQuickWebEngineViewPrivate::requestFullScreenMode(const QUrl &origin, bool fullscreen)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineFullScreenRequest request(this, origin, fullscreen);
    Q_EMIT q->fullScreenRequested(request);
}

bool QQuickWebEngineViewPrivate::isFullScreenMode() const
{
    return m_fullscreenMode;
}

void QQuickWebEngineViewPrivate::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
{
    Q_Q(QQuickWebEngineView);
    if (q->receivers(SIGNAL(javaScriptConsoleMessage(JavaScriptConsoleMessageLevel,QString,int,QString))) > 0) {
        Q_EMIT q->javaScriptConsoleMessage(static_cast<QQuickWebEngineView::JavaScriptConsoleMessageLevel>(level), message, lineNumber, sourceID);
        return;
    }

    static QLoggingCategory loggingCategory("js", QtWarningMsg);
    const QByteArray file = sourceID.toUtf8();
    QMessageLogger logger(file.constData(), lineNumber, nullptr, loggingCategory.categoryName());

    switch (level) {
    case JavaScriptConsoleMessageLevel::Info:
        if (loggingCategory.isInfoEnabled())
            logger.info().noquote() << message;
        break;
    case JavaScriptConsoleMessageLevel::Warning:
        if (loggingCategory.isWarningEnabled())
            logger.warning().noquote() << message;
        break;
    case JavaScriptConsoleMessageLevel::Error:
        if (loggingCategory.isCriticalEnabled())
            logger.critical().noquote() << message;
        break;
    }
}

void QQuickWebEngineViewPrivate::authenticationRequired(QSharedPointer<AuthenticationDialogController> controller)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineAuthenticationDialogRequest *request = new QQuickWebEngineAuthenticationDialogRequest(controller);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->authenticationDialogRequested(request);
    if (!request->isAccepted())
        ui()->showDialog(controller);
}

void QQuickWebEngineViewPrivate::runMediaAccessPermissionRequest(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags requestFlags)
{
    Q_Q(QQuickWebEngineView);
    if (!requestFlags)
        return;
    QQuickWebEngineView::Feature feature;
    if (requestFlags.testFlag(WebContentsAdapterClient::MediaAudioCapture) && requestFlags.testFlag(WebContentsAdapterClient::MediaVideoCapture))
        feature = QQuickWebEngineView::MediaAudioVideoCapture;
    else if (requestFlags.testFlag(WebContentsAdapterClient::MediaAudioCapture))
        feature = QQuickWebEngineView::MediaAudioCapture;
    else // WebContentsAdapterClient::MediaVideoCapture
        feature = QQuickWebEngineView::MediaVideoCapture;
    Q_EMIT q->featurePermissionRequested(securityOrigin, feature);
}

void QQuickWebEngineViewPrivate::runMouseLockPermissionRequest(const QUrl &securityOrigin)
{

    Q_UNUSED(securityOrigin);

    // TODO: Add mouse lock support
    adapter->grantMouseLockPermission(false);
}

#ifndef QT_NO_ACCESSIBILITY
QObject *QQuickWebEngineViewPrivate::accessibilityParentObject()
{
    Q_Q(QQuickWebEngineView);
    return q;
}
#endif // QT_NO_ACCESSIBILITY

QSharedPointer<BrowserContextAdapter> QQuickWebEngineViewPrivate::browserContextAdapter()
{
    return m_profile->d_ptr->browserContext();
}

WebContentsAdapter *QQuickWebEngineViewPrivate::webContentsAdapter()
{
    return adapter.data();
}

WebEngineSettings *QQuickWebEngineViewPrivate::webEngineSettings() const
{
    return m_settings->d_ptr.data();
}

const QObject *QQuickWebEngineViewPrivate::holdingQObject() const
{
    Q_Q(const QQuickWebEngineView);
    return q;
}

void QQuickWebEngineViewPrivate::setDevicePixelRatio(qreal devicePixelRatio)
{
    Q_Q(QQuickWebEngineView);
    this->devicePixelRatio = devicePixelRatio;
    QScreen *screen = q->window() ? q->window()->screen() : QGuiApplication::primaryScreen();
    m_dpiScale = devicePixelRatio / screen->devicePixelRatio();
}

#ifndef QT_NO_ACCESSIBILITY
QQuickWebEngineViewAccessible::QQuickWebEngineViewAccessible(QQuickWebEngineView *o)
    : QAccessibleObject(o)
{}

QAccessibleInterface *QQuickWebEngineViewAccessible::parent() const
{
    QQuickItem *parent = engineView()->parentItem();
    return QAccessible::queryAccessibleInterface(parent);
}

int QQuickWebEngineViewAccessible::childCount() const
{
    if (engineView() && child(0))
        return 1;
    return 0;
}

QAccessibleInterface *QQuickWebEngineViewAccessible::child(int index) const
{
    if (index == 0)
        return engineView()->d_func()->adapter->browserAccessible();
    return 0;
}

int QQuickWebEngineViewAccessible::indexOfChild(const QAccessibleInterface *c) const
{
    if (c == child(0))
        return 0;
    return -1;
}

QString QQuickWebEngineViewAccessible::text(QAccessible::Text) const
{
    return QString();
}

QAccessible::Role QQuickWebEngineViewAccessible::role() const
{
    return QAccessible::Document;
}

QAccessible::State QQuickWebEngineViewAccessible::state() const
{
    QAccessible::State s;
    return s;
}
#endif // QT_NO_ACCESSIBILITY

class WebContentsAdapterOwner : public QObject
{
public:
    typedef QSharedPointer<QtWebEngineCore::WebContentsAdapter> AdapterPtr;
    WebContentsAdapterOwner(const AdapterPtr &ptr)
        : adapter(ptr)
    {}

private:
    AdapterPtr adapter;
};

void QQuickWebEngineViewPrivate::adoptWebContents(WebContentsAdapter *webContents)
{
    if (!webContents) {
        qWarning("Trying to open an empty request, it was either already used or was invalidated."
            "\nYou must complete the request synchronously within the newViewRequested signal handler."
            " If a view hasn't been adopted before returning, the request will be invalidated.");
        return;
    }

    if (webContents->browserContextAdapter() && browserContextAdapter() != webContents->browserContextAdapter()) {
        qWarning("Can not adopt content from a different WebEngineProfile.");
        return;
    }

    Q_Q(QQuickWebEngineView);

    // This throws away the WebContentsAdapter that has been used until now.
    // All its states, particularly the loading URL, are replaced by the adopted WebContentsAdapter.
    if (adapter) {
        WebContentsAdapterOwner *adapterOwner = new WebContentsAdapterOwner(adapter->sharedFromThis());
        adapterOwner->deleteLater();
    }
    adapter = webContents->sharedFromThis();
    adapter->initialize(this);

    // associate the webChannel with the new adapter
    if (m_webChannel)
        adapter->setWebChannel(m_webChannel, m_webChannelWorld);

    // set initial background color if non-default
    if (m_backgroundColor != Qt::white)
        adapter->backgroundColorChanged();

    // re-bind the userscrips to the new adapter
    Q_FOREACH (QQuickWebEngineScript *script, m_userScripts)
        script->d_func()->bind(browserContextAdapter()->userResourceController(), adapter.data());

    // set the zoomFactor if it had been changed on the old adapter.
    if (!qFuzzyCompare(adapter->currentZoomFactor(), m_defaultZoomFactor))
        q->setZoomFactor(m_defaultZoomFactor);

    // Emit signals for values that might be different from the previous WebContentsAdapter.
    emit q->titleChanged();
    emit q->urlChanged();
    emit q->iconChanged();
    // FIXME: The current loading state should be stored in the WebContentAdapter
    // and it should be checked here if the signal emission is really necessary.
    QQuickWebEngineLoadRequest loadRequest(adapter->activeUrl(), QQuickWebEngineView::LoadSucceededStatus);
    emit q->loadingChanged(&loadRequest);
    emit q->loadProgressChanged();
}

QQuickWebEngineView::QQuickWebEngineView(QQuickItem *parent)
    : QQuickItem(parent)
    , d_ptr(new QQuickWebEngineViewPrivate)
{
    Q_D(QQuickWebEngineView);
    d->q_ptr = this;
    this->setActiveFocusOnTab(true);
    this->setFlags(QQuickItem::ItemIsFocusScope | QQuickItem::ItemAcceptsInputMethod
                   | QQuickItem::ItemAcceptsDrops);

#ifndef QT_NO_ACCESSIBILITY
    QQuickAccessibleAttached *accessible = QQuickAccessibleAttached::qmlAttachedProperties(this);
    accessible->setRole(QAccessible::Grouping);
#endif // QT_NO_ACCESSIBILITY
}

QQuickWebEngineView::~QQuickWebEngineView()
{
    Q_D(QQuickWebEngineView);
    if (d->faviconProvider)
        d->faviconProvider->detach(this);
}

void QQuickWebEngineViewPrivate::ensureContentsAdapter()
{
    Q_Q(QQuickWebEngineView);
    if (!adapter) {
        adapter = QSharedPointer<WebContentsAdapter>::create();
        adapter->initialize(this);
        if (m_backgroundColor != Qt::white)
            adapter->backgroundColorChanged();
        if (m_webChannel)
            adapter->setWebChannel(m_webChannel, m_webChannelWorld);
        if (explicitUrl.isValid())
            adapter->load(explicitUrl);
        // push down the page's user scripts
        Q_FOREACH (QQuickWebEngineScript *script, m_userScripts)
            script->d_func()->bind(browserContextAdapter()->userResourceController(), adapter.data());
        // set the zoomFactor if it had been changed on the old adapter.
        if (!qFuzzyCompare(adapter->currentZoomFactor(), m_defaultZoomFactor))
            q->setZoomFactor(m_defaultZoomFactor);

    }
}

void QQuickWebEngineViewPrivate::setFullScreenMode(bool fullscreen)
{
    Q_Q(QQuickWebEngineView);
    if (m_fullscreenMode != fullscreen) {
        m_fullscreenMode = fullscreen;
        adapter->changedFullScreen();
        Q_EMIT q->isFullScreenChanged();
    }
}

QUrl QQuickWebEngineView::url() const
{
    Q_D(const QQuickWebEngineView);
    return d->explicitUrl.isValid() ? d->explicitUrl : (d->adapter ? d->adapter->activeUrl() : QUrl());
}

void QQuickWebEngineView::setUrl(const QUrl& url)
{
    if (url.isEmpty())
        return;

    Q_D(QQuickWebEngineView);
    d->explicitUrl = url;
    if (d->adapter)
        d->adapter->load(url);
    if (!qmlEngine(this) || isComponentComplete())
        d->ensureContentsAdapter();
}

QUrl QQuickWebEngineView::icon() const
{
    Q_D(const QQuickWebEngineView);
    return d->iconUrl;
}

void QQuickWebEngineView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_D(QQuickWebEngineView);
    d->explicitUrl = QUrl();
    if (!qmlEngine(this) || isComponentComplete())
        d->ensureContentsAdapter();
    if (d->adapter)
        d->adapter->setContent(html.toUtf8(), QStringLiteral("text/html;charset=UTF-8"), baseUrl);
}

void QQuickWebEngineView::goBack()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->navigateToOffset(-1);
}

void QQuickWebEngineView::goForward()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->navigateToOffset(1);
}

void QQuickWebEngineView::reload()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->reload();
}

void QQuickWebEngineView::reloadAndBypassCache()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->reloadAndBypassCache();
}

void QQuickWebEngineView::stop()
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    d->adapter->stop();
}

void QQuickWebEngineView::setZoomFactor(qreal arg)
{
    Q_D(QQuickWebEngineView);
    d->m_defaultZoomFactor = arg;

    if (!d->adapter)
        return;

    qreal oldFactor = d->adapter->currentZoomFactor();
    d->adapter->setZoomFactor(arg);
    if (qFuzzyCompare(oldFactor, d->adapter->currentZoomFactor()))
        return;

    emit zoomFactorChanged(arg);
}

QQuickWebEngineProfile *QQuickWebEngineView::profile() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_profile;
}

void QQuickWebEngineView::setProfile(QQuickWebEngineProfile *profile)
{
    Q_D(QQuickWebEngineView);
    d->setProfile(profile);
}

QQuickWebEngineSettings *QQuickWebEngineView::settings() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_settings.data();
}

QQmlListProperty<QQuickWebEngineScript> QQuickWebEngineView::userScripts()
{
    Q_D(QQuickWebEngineView);
    return QQmlListProperty<QQuickWebEngineScript>(this, d,
                                                   d->userScripts_append,
                                                   d->userScripts_count,
                                                   d->userScripts_at,
                                                   d->userScripts_clear);
}

void QQuickWebEngineViewPrivate::setProfile(QQuickWebEngineProfile *profile)
{
    Q_Q(QQuickWebEngineView);

    if (profile == m_profile)
        return;
    m_profile = profile;
    Q_EMIT q->profileChanged();
    m_settings->setParentSettings(profile->settings());

    if (adapter && adapter->browserContext() != browserContextAdapter()->browserContext()) {
        // When the profile changes we need to create a new WebContentAdapter and reload the active URL.
        QUrl activeUrl = adapter->activeUrl();
        adapter.reset();
        ensureContentsAdapter();

        if (!explicitUrl.isValid() && activeUrl.isValid())
            adapter->load(activeUrl);
    }
}

#ifdef ENABLE_QML_TESTSUPPORT_API
QQuickWebEngineTestSupport *QQuickWebEngineView::testSupport() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_testSupport;
}

void QQuickWebEngineView::setTestSupport(QQuickWebEngineTestSupport *testSupport)
{
    Q_D(QQuickWebEngineView);
    d->m_testSupport = testSupport;
}

#endif

bool QQuickWebEngineView::activeFocusOnPress() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_activeFocusOnPress;
}

void QQuickWebEngineViewPrivate::didRunJavaScript(quint64 requestId, const QVariant &result)
{
    Q_Q(QQuickWebEngineView);
    QJSValue callback = m_callbacks.take(requestId);
    QJSValueList args;
    args.append(qmlEngine(q)->toScriptValue(result));
    callback.call(args);
}

void QQuickWebEngineViewPrivate::didFindText(quint64 requestId, int matchCount)
{
    QJSValue callback = m_callbacks.take(requestId);
    QJSValueList args;
    args.append(QJSValue(matchCount));
    callback.call(args);
}

void QQuickWebEngineViewPrivate::didPrintPage(quint64 requestId, const QByteArray &result)
{
    QJSValue callback = m_callbacks.take(requestId);
    QJSValueList args;
    args.append(QJSValue(result.data()));
    callback.call(args);
}

void QQuickWebEngineViewPrivate::showValidationMessage(const QRect &anchor, const QString &mainText, const QString &subText)
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    if (m_testSupport)
        Q_EMIT m_testSupport->validationMessageShown(mainText, subText);
#endif
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineFormValidationMessageRequest *request;
    request = new QQuickWebEngineFormValidationMessageRequest(QQuickWebEngineFormValidationMessageRequest::Show,
                                                          anchor,mainText,subText);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->formValidationMessageRequested(request);
    if (!request->isAccepted())
        ui()->showMessageBubble(anchor, mainText, subText);
}

void QQuickWebEngineViewPrivate::hideValidationMessage()
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineFormValidationMessageRequest *request;
    request = new QQuickWebEngineFormValidationMessageRequest(QQuickWebEngineFormValidationMessageRequest::Hide);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->formValidationMessageRequested(request);
    if (!request->isAccepted())
        ui()->hideMessageBubble();
}

void QQuickWebEngineViewPrivate::moveValidationMessage(const QRect &anchor)
{
    Q_Q(QQuickWebEngineView);
    QQuickWebEngineFormValidationMessageRequest *request;
    request = new QQuickWebEngineFormValidationMessageRequest(QQuickWebEngineFormValidationMessageRequest::Move,
                                                          anchor);
    // mark the object for gc by creating temporary jsvalue
    qmlEngine(q)->newQObject(request);
    Q_EMIT q->formValidationMessageRequested(request);
    if (!request->isAccepted())
        ui()->moveMessageBubble(anchor);
}

void QQuickWebEngineViewPrivate::updateScrollPosition(const QPointF &position)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->scrollPositionChanged(position);
}

void QQuickWebEngineViewPrivate::updateContentsSize(const QSizeF &size)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->contentsSizeChanged(size);
}

void QQuickWebEngineViewPrivate::renderProcessTerminated(
        RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    Q_Q(QQuickWebEngineView);
    Q_EMIT q->renderProcessTerminated(static_cast<QQuickWebEngineView::RenderProcessTerminationStatus>(
                                      renderProcessExitStatus(terminationStatus)), exitCode);
}

void QQuickWebEngineViewPrivate::startDragging(const content::DropData &dropData,
                                               Qt::DropActions allowedActions,
                                               const QPixmap &pixmap, const QPoint &offset)
{
    adapter->startDragging(q_ptr->window(), dropData, allowedActions, pixmap, offset);
}

bool QQuickWebEngineViewPrivate::isEnabled() const
{
    const Q_Q(QQuickWebEngineView);
    return q->isEnabled();
}

void QQuickWebEngineViewPrivate::setToolTip(const QString &toolTipText)
{
    ui()->showToolTip(toolTipText);
}

bool QQuickWebEngineView::isLoading() const
{
    Q_D(const QQuickWebEngineView);
    return d->isLoading;
}

int QQuickWebEngineView::loadProgress() const
{
    Q_D(const QQuickWebEngineView);
    return d->loadProgress;
}

QString QQuickWebEngineView::title() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return QString();
    return d->adapter->pageTitle();
}

bool QQuickWebEngineView::canGoBack() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return false;
    return d->adapter->canGoBack();
}

bool QQuickWebEngineView::canGoForward() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return false;
    return d->adapter->canGoForward();
}

void QQuickWebEngineView::runJavaScript(const QString &script, const QJSValue &callback)
{
    Q_D(QQuickWebEngineView);
    d->ensureContentsAdapter();
    if (!callback.isUndefined()) {
        quint64 requestId = d_ptr->adapter->runJavaScriptCallbackResult(script, QQuickWebEngineScript::MainWorld);
        d->m_callbacks.insert(requestId, callback);
    } else
        d->adapter->runJavaScript(script, QQuickWebEngineScript::MainWorld);
}

void QQuickWebEngineView::runJavaScript(const QString &script, quint32 worldId, const QJSValue &callback)
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    if (!callback.isUndefined()) {
        quint64 requestId = d_ptr->adapter->runJavaScriptCallbackResult(script, worldId);
        d->m_callbacks.insert(requestId, callback);
    } else
        d->adapter->runJavaScript(script, worldId);
}

qreal QQuickWebEngineView::zoomFactor() const
{
    Q_D(const QQuickWebEngineView);
    if (!d->adapter)
        return d->m_defaultZoomFactor;
    return d->adapter->currentZoomFactor();
}

QColor QQuickWebEngineView::backgroundColor() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_backgroundColor;
}

void QQuickWebEngineView::setBackgroundColor(const QColor &color)
{
    Q_D(QQuickWebEngineView);
    if (color == d->m_backgroundColor)
        return;
    d->m_backgroundColor = color;
    if (d->adapter)
        d->adapter->backgroundColorChanged();
    emit backgroundColorChanged();
}

/*!
    \property QQuickWebEngineView::audioMuted
    \brief the state of whether the current page audio is muted.
    \since 5.7

    The default value is false.
*/
bool QQuickWebEngineView::isAudioMuted() const
{
    const Q_D(QQuickWebEngineView);
    if (d->adapter)
        return d->adapter->isAudioMuted();
    return false;
}

void QQuickWebEngineView::setAudioMuted(bool muted)
{
    Q_D(QQuickWebEngineView);
    bool _isAudioMuted = isAudioMuted();
    if (d->adapter) {
        d->adapter->setAudioMuted(muted);
        if (_isAudioMuted != muted) {
            Q_EMIT audioMutedChanged(muted);
        }
    }
}

bool QQuickWebEngineView::recentlyAudible() const
{
    const Q_D(QQuickWebEngineView);
    if (d->adapter)
        return d->adapter->recentlyAudible();
    return false;
}

void QQuickWebEngineView::printToPdf(const QString& filePath, PrintedPageSizeId pageSizeId, PrintedPageOrientation orientation)
{
#if defined(ENABLE_PDF)
    Q_D(const QQuickWebEngineView);
    QPageSize layoutSize(static_cast<QPageSize::PageSizeId>(pageSizeId));
    QPageLayout::Orientation layoutOrientation = static_cast<QPageLayout::Orientation>(orientation);
    QPageLayout pageLayout(layoutSize, layoutOrientation, QMarginsF(0.0, 0.0, 0.0, 0.0));

    d->adapter->printToPDF(pageLayout, filePath);
#else
    Q_UNUSED(filePath);
    Q_UNUSED(pageSizeId);
    Q_UNUSED(orientation);
#endif
}

void QQuickWebEngineView::printToPdf(const QJSValue &callback, PrintedPageSizeId pageSizeId, PrintedPageOrientation orientation)
{
#if defined (ENABLE_PDF)
    Q_D(QQuickWebEngineView);
    QPageSize layoutSize(static_cast<QPageSize::PageSizeId>(pageSizeId));
    QPageLayout::Orientation layoutOrientation = static_cast<QPageLayout::Orientation>(orientation);
    QPageLayout pageLayout(layoutSize, layoutOrientation, QMarginsF(0.0, 0.0, 0.0, 0.0));

    if (callback.isUndefined())
        return;

    quint64 requestId = d->adapter->printToPDFCallbackResult(pageLayout);
    d->m_callbacks.insert(requestId, callback);
#else
    Q_UNUSED(pageSizeId);
    Q_UNUSED(orientation);

    // Call back with null result.
    QJSValueList args;
    args.append(QJSValue(QByteArray().data()));
    QJSValue callbackCopy = callback;
    callbackCopy.call(args);
#endif
}

void QQuickWebEngineView::replaceMisspelledWord(const QString &replacement)
{
    Q_D(QQuickWebEngineView);
    d->adapter->replaceMisspelling(replacement);
}

bool QQuickWebEngineView::isFullScreen() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_fullscreenMode;
}

void QQuickWebEngineView::findText(const QString &subString, FindFlags options, const QJSValue &callback)
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    if (subString.isEmpty()) {
        d->adapter->stopFinding();
        if (!callback.isUndefined()) {
            QJSValueList args;
            args.append(QJSValue(0));
            const_cast<QJSValue&>(callback).call(args);
        }
    } else {
        quint64 requestId = d->adapter->findText(subString, options & FindCaseSensitively, options & FindBackward);
        if (!callback.isUndefined())
            d->m_callbacks.insert(requestId, callback);
    }
}

QQuickWebEngineHistory *QQuickWebEngineView::navigationHistory() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_history.data();
}

QQmlWebChannel *QQuickWebEngineView::webChannel()
{
    Q_D(QQuickWebEngineView);
    if (!d->m_webChannel) {
        d->m_webChannel = new QQmlWebChannel(this);
        if (d->adapter)
            d->adapter->setWebChannel(d->m_webChannel, d->m_webChannelWorld);
    }

    return d->m_webChannel;
}

void QQuickWebEngineView::setWebChannel(QQmlWebChannel *webChannel)
{
    Q_D(QQuickWebEngineView);
    if (d->m_webChannel == webChannel)
        return;
    d->m_webChannel = webChannel;
    if (d->adapter)
        d->adapter->setWebChannel(webChannel, d->m_webChannelWorld);
    Q_EMIT webChannelChanged();
}

uint QQuickWebEngineView::webChannelWorld() const
{
    Q_D(const QQuickWebEngineView);
    return d->m_webChannelWorld;
}

void QQuickWebEngineView::setWebChannelWorld(uint webChannelWorld)
{
    Q_D(QQuickWebEngineView);
    if (d->m_webChannelWorld == webChannelWorld)
        return;
    d->m_webChannelWorld = webChannelWorld;
    if (d->adapter)
        d->adapter->setWebChannel(d->m_webChannel, d->m_webChannelWorld);
    Q_EMIT webChannelWorldChanged(webChannelWorld);
}

void QQuickWebEngineView::grantFeaturePermission(const QUrl &securityOrigin, QQuickWebEngineView::Feature feature, bool granted)
{
    if (!d_ptr->adapter)
        return;
    if (!granted && feature >= MediaAudioCapture && feature <= MediaAudioVideoCapture) {
         d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaNone);
         return;
    }

    switch (feature) {
    case MediaAudioCapture:
        d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaAudioCapture);
        break;
    case MediaVideoCapture:
        d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaVideoCapture);
        break;
    case MediaAudioVideoCapture:
        d_ptr->adapter->grantMediaAccessPermission(securityOrigin, WebContentsAdapterClient::MediaRequestFlags(WebContentsAdapterClient::MediaAudioCapture                                                                                                               | WebContentsAdapterClient::MediaVideoCapture));
        break;
    case Geolocation:
        d_ptr->adapter->runGeolocationRequestCallback(securityOrigin, granted);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void QQuickWebEngineView::setActiveFocusOnPress(bool arg)
{
    Q_D(QQuickWebEngineView);
    if (d->m_activeFocusOnPress == arg)
        return;

    d->m_activeFocusOnPress = arg;
    emit activeFocusOnPressChanged(arg);
}

void QQuickWebEngineView::goBackOrForward(int offset)
{
    Q_D(QQuickWebEngineView);
    if (!d->adapter)
        return;
    const int current = d->adapter->currentNavigationEntryIndex();
    const int count = d->adapter->navigationEntryCount();
    const int index = current + offset;

    if (index < 0 || index >= count)
        return;

    d->adapter->navigateToIndex(index);
}

void QQuickWebEngineView::fullScreenCancelled()
{
    Q_D(QQuickWebEngineView);
    d->adapter->exitFullScreen();
}

void QQuickWebEngineView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    Q_FOREACH(QQuickItem *child, childItems()) {
        if (qobject_cast<RenderWidgetHostViewQtDelegateQuick *>(child))
            child->setSize(newGeometry.size());
    }
}

void QQuickWebEngineView::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(QQuickWebEngineView);
    if (d->adapter && (change == ItemSceneChange || change == ItemVisibleHasChanged)) {
        if (window() && isVisible())
            d->adapter->wasShown();
        else
            d->adapter->wasHidden();
    }
    QQuickItem::itemChange(change, value);
}

static QPoint mapToScreen(const QQuickItem *item, const QPoint &clientPos)
{
    return item->window()->position() + item->mapToScene(clientPos).toPoint();
}

void QQuickWebEngineView::dragEnterEvent(QDragEnterEvent *e)
{
    Q_D(QQuickWebEngineView);
    e->accept();
    d->adapter->enterDrag(e, mapToScreen(this, e->pos()));
}

void QQuickWebEngineView::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_D(QQuickWebEngineView);
    e->accept();
    d->adapter->leaveDrag();
}

void QQuickWebEngineView::dragMoveEvent(QDragMoveEvent *e)
{
    Q_D(QQuickWebEngineView);
    e->accept();
    d->adapter->updateDragPosition(e, mapToScreen(this, e->pos()));
}

void QQuickWebEngineView::dropEvent(QDropEvent *e)
{
    Q_D(QQuickWebEngineView);
    e->accept();
    d->adapter->endDragging(e->pos(), mapToScreen(this, e->pos()));
}

void QQuickWebEngineView::triggerWebAction(WebAction action)
{
    Q_D(QQuickWebEngineView);
    switch (action) {
    case Back:
        d->adapter->navigateToOffset(-1);
        break;
    case Forward:
        d->adapter->navigateToOffset(1);
        break;
    case Stop:
        d->adapter->stop();
        break;
    case Reload:
        d->adapter->reload();
        break;
    case ReloadAndBypassCache:
        d->adapter->reloadAndBypassCache();
        break;
    case Cut:
        d->adapter->cut();
        break;
    case Copy:
        d->adapter->copy();
        break;
    case Paste:
        d->adapter->paste();
        break;
    case Undo:
        d->adapter->undo();
        break;
    case Redo:
        d->adapter->redo();
        break;
    case SelectAll:
        d->adapter->selectAll();
        break;
    case PasteAndMatchStyle:
        d->adapter->pasteAndMatchStyle();
        break;
    case Unselect:
        d->adapter->unselect();
        break;
    case OpenLinkInThisWindow:
        if (d->m_contextMenuData.linkUrl().isValid())
            setUrl(d->m_contextMenuData.linkUrl());
        break;
    case OpenLinkInNewWindow:
        if (d->m_contextMenuData.linkUrl().isValid()) {
            QQuickWebEngineNewViewRequest request;
            request.m_requestedUrl = d->m_contextMenuData.linkUrl();
            request.m_isUserInitiated = true;
            request.m_destination = NewViewInWindow;
            Q_EMIT newViewRequested(&request);
        }
        break;
    case OpenLinkInNewTab:
        if (d->m_contextMenuData.linkUrl().isValid()) {
            QQuickWebEngineNewViewRequest request;
            request.m_requestedUrl = d->m_contextMenuData.linkUrl();
            request.m_isUserInitiated = true;
            request.m_destination = NewViewInBackgroundTab;
            Q_EMIT newViewRequested(&request);
        }
        break;
    case CopyLinkToClipboard:
        if (d->m_contextMenuData.linkUrl().isValid()) {
            QString urlString = d->m_contextMenuData.linkUrl().toString(QUrl::FullyEncoded);
            QString title = d->m_contextMenuData.linkText().toHtmlEscaped();
            QMimeData *data = new QMimeData();
            data->setText(urlString);
            QString html = QStringLiteral("<a href=\"") + urlString + QStringLiteral("\">") + title + QStringLiteral("</a>");
            data->setHtml(html);
            data->setUrls(QList<QUrl>() << d->m_contextMenuData.linkUrl());
            qApp->clipboard()->setMimeData(data);
        }
        break;
    case DownloadLinkToDisk:
        if (d->m_contextMenuData.linkUrl().isValid())
            d->adapter->download(d->m_contextMenuData.linkUrl(), d->m_contextMenuData.suggestedFileName());
        break;
    case CopyImageToClipboard:
        if (d->m_contextMenuData.hasImageContent() &&
                (d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeImage ||
                 d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeCanvas))
        {
            d->adapter->copyImageAt(d->m_contextMenuData.position());
        }
        break;
    case CopyImageUrlToClipboard:
        if (d->m_contextMenuData.mediaUrl().isValid() && d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeImage) {
            QString urlString = d->m_contextMenuData.mediaUrl().toString(QUrl::FullyEncoded);
            QString title = d->m_contextMenuData.linkText();
            if (!title.isEmpty())
                title = QStringLiteral(" alt=\"%1\"").arg(title.toHtmlEscaped());
            QMimeData *data = new QMimeData();
            data->setText(urlString);
            QString html = QStringLiteral("<img src=\"") + urlString + QStringLiteral("\"") + title + QStringLiteral("></img>");
            data->setHtml(html);
            data->setUrls(QList<QUrl>() << d->m_contextMenuData.mediaUrl());
            qApp->clipboard()->setMimeData(data);
        }
        break;
    case DownloadImageToDisk:
    case DownloadMediaToDisk:
        if (d->m_contextMenuData.mediaUrl().isValid())
            d->adapter->download(d->m_contextMenuData.mediaUrl(), d->m_contextMenuData.suggestedFileName());
        break;
    case CopyMediaUrlToClipboard:
        if (d->m_contextMenuData.mediaUrl().isValid() &&
                (d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeAudio ||
                 d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeVideo))
        {
            QString urlString = d->m_contextMenuData.mediaUrl().toString(QUrl::FullyEncoded);
            QMimeData *data = new QMimeData();
            data->setText(urlString);
            if (d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeAudio)
                data->setHtml(QStringLiteral("<audio src=\"") + urlString + QStringLiteral("\"></audio>"));
            else
                data->setHtml(QStringLiteral("<video src=\"") + urlString + QStringLiteral("\"></video>"));
            data->setUrls(QList<QUrl>() << d->m_contextMenuData.mediaUrl());
            qApp->clipboard()->setMimeData(data);
        }
        break;
    case ToggleMediaControls:
        if (d->m_contextMenuData.mediaUrl().isValid() && d->m_contextMenuData.mediaFlags() & WebEngineContextMenuData::MediaCanToggleControls) {
            bool enable = !(d->m_contextMenuData.mediaFlags() & WebEngineContextMenuData::MediaControls);
            d->adapter->executeMediaPlayerActionAt(d->m_contextMenuData.position(), WebContentsAdapter::MediaPlayerControls, enable);
        }
        break;
    case ToggleMediaLoop:
        if (d->m_contextMenuData.mediaUrl().isValid() &&
                (d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeAudio ||
                 d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeVideo))
        {
            bool enable = !(d->m_contextMenuData.mediaFlags() & WebEngineContextMenuData::MediaLoop);
            d->adapter->executeMediaPlayerActionAt(d->m_contextMenuData.position(), WebContentsAdapter::MediaPlayerLoop, enable);
        }
        break;
    case ToggleMediaPlayPause:
        if (d->m_contextMenuData.mediaUrl().isValid() &&
                (d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeAudio ||
                 d->m_contextMenuData.mediaType() == WebEngineContextMenuData::MediaTypeVideo))
        {
            bool enable = (d->m_contextMenuData.mediaFlags() & WebEngineContextMenuData::MediaPaused);
            d->adapter->executeMediaPlayerActionAt(d->m_contextMenuData.position(), WebContentsAdapter::MediaPlayerPlay, enable);
        }
        break;
    case ToggleMediaMute:
        if (d->m_contextMenuData.mediaUrl().isValid() && d->m_contextMenuData.mediaFlags() & WebEngineContextMenuData::MediaHasAudio) {
            bool enable = !(d->m_contextMenuData.mediaFlags() & WebEngineContextMenuData::MediaMuted);
            d->adapter->executeMediaPlayerActionAt(d->m_contextMenuData.position(), WebContentsAdapter::MediaPlayerMute, enable);
        }
        break;
    case InspectElement:
        d->adapter->inspectElementAt(d->m_contextMenuData.position());
        break;
    case ExitFullScreen:
        d->adapter->exitFullScreen();
        break;
    case RequestClose:
        d->adapter->requestClose();
        break;
    case SavePage:
        d->adapter->save();
        break;
    case ViewSource:
        d->adapter->viewSource();
        break;
    default:
        Q_UNREACHABLE();
    }
}

QSizeF QQuickWebEngineView::contentsSize() const
{
    Q_D(const QQuickWebEngineView);
    return d->adapter->lastContentsSize();
}

QPointF QQuickWebEngineView::scrollPosition() const
{
    Q_D(const QQuickWebEngineView);
    return d->adapter->lastScrollOffset();
}

void QQuickWebEngineViewPrivate::userScripts_append(QQmlListProperty<QQuickWebEngineScript> *p, QQuickWebEngineScript *script)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    UserResourceControllerHost *resourceController = d->browserContextAdapter()->userResourceController();
    d->m_userScripts.append(script);
    // If the adapter hasn't been instantiated, we'll bind the scripts in ensureContentsAdapter()
    if (!d->adapter)
        return;
    script->d_func()->bind(resourceController, d->adapter.data());
}

int QQuickWebEngineViewPrivate::userScripts_count(QQmlListProperty<QQuickWebEngineScript> *p)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    return d->m_userScripts.count();
}

QQuickWebEngineScript *QQuickWebEngineViewPrivate::userScripts_at(QQmlListProperty<QQuickWebEngineScript> *p, int idx)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    return d->m_userScripts.at(idx);
}

void QQuickWebEngineViewPrivate::userScripts_clear(QQmlListProperty<QQuickWebEngineScript> *p)
{
    Q_ASSERT(p && p->data);
    QQuickWebEngineViewPrivate *d = static_cast<QQuickWebEngineViewPrivate*>(p->data);
    UserResourceControllerHost *resourceController = d->browserContextAdapter()->userResourceController();
    resourceController->clearAllScripts(d->adapter.data());
    d->m_userScripts.clear();
}

void QQuickWebEngineView::componentComplete()
{
    QQuickItem::componentComplete();
    QTimer::singleShot(0, this, &QQuickWebEngineView::lazyInitialize);
}

void QQuickWebEngineView::lazyInitialize()
{
    Q_D(QQuickWebEngineView);
    d->ensureContentsAdapter();
}

QQuickWebEngineFullScreenRequest::QQuickWebEngineFullScreenRequest()
    : m_viewPrivate(0)
    , m_toggleOn(false)
{
}

QQuickWebEngineFullScreenRequest::QQuickWebEngineFullScreenRequest(QQuickWebEngineViewPrivate *viewPrivate, const QUrl &origin, bool toggleOn)
    : m_viewPrivate(viewPrivate)
    , m_origin(origin)
    , m_toggleOn(toggleOn)
{
}

void QQuickWebEngineFullScreenRequest::accept()
{
    if (m_viewPrivate)
        m_viewPrivate->setFullScreenMode(m_toggleOn);
}

void QQuickWebEngineFullScreenRequest::reject()
{
    if (m_viewPrivate)
        m_viewPrivate->setFullScreenMode(!m_toggleOn);
}

QT_END_NAMESPACE

