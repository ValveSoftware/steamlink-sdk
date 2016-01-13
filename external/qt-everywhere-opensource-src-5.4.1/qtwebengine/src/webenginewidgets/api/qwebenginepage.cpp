/*
    Copyright (C) 2012, 2013 Digia Plc and/or its subsidiary(-ies).
    Copyright (C) 2008, 2009, 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.
    Copyright (C) 2007 Apple Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "qwebenginepage.h"
#include "qwebenginepage_p.h"

#include "certificate_error_controller.h"
#include "javascript_dialog_controller.h"
#include "qwebenginehistory.h"
#include "qwebenginehistory_p.h"
#include "qwebenginesettings.h"
#include "qwebenginesettings_p.h"
#include "qwebengineview.h"
#include "qwebengineview_p.h"
#include "render_widget_host_view_qt_delegate_widget.h"
#include "web_contents_adapter.h"
#include "web_engine_settings.h"

#include <QAction>
#include <QApplication>
#include <QAuthenticator>
#include <QClipboard>
#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>

QT_BEGIN_NAMESPACE

static QWebEnginePage::WebWindowType toWindowType(WebContentsAdapterClient::WindowOpenDisposition disposition)
{
    switch (disposition) {
    case WebContentsAdapterClient::NewForegroundTabDisposition:
    case WebContentsAdapterClient::NewBackgroundTabDisposition:
        return QWebEnginePage::WebBrowserTab;
    case WebContentsAdapterClient::NewPopupDisposition:
        return QWebEnginePage::WebDialog;
    case WebContentsAdapterClient::NewWindowDisposition:
        return QWebEnginePage::WebBrowserWindow;
    default:
        Q_UNREACHABLE();
    }
}

CallbackDirectory::~CallbackDirectory()
{
    // "Cancel" pending callbacks by calling them with an invalid value.
    // This guarantees that each callback is called exactly once.
    Q_FOREACH (const CallbackSharedDataPointer &sharedPtr, m_callbackMap) {
        switch (sharedPtr.type) {
        case CallbackSharedDataPointer::Variant:
            (*sharedPtr.variantCallback)(QVariant());
            break;
        case CallbackSharedDataPointer::String:
            (*sharedPtr.stringCallback)(QString());
            break;
        case CallbackSharedDataPointer::Bool:
            (*sharedPtr.boolCallback)(false);
            break;
        default:
            Q_UNREACHABLE();
        }
    }
}

void CallbackDirectory::registerCallback(quint64 requestId, const QExplicitlySharedDataPointer<VariantCallback> &callback)
{
    m_callbackMap.insert(requestId, CallbackSharedDataPointer(callback.data()));
}

void CallbackDirectory::registerCallback(quint64 requestId, const QExplicitlySharedDataPointer<StringCallback> &callback)
{
    m_callbackMap.insert(requestId, CallbackSharedDataPointer(callback.data()));
}

void CallbackDirectory::registerCallback(quint64 requestId, const QExplicitlySharedDataPointer<BoolCallback> &callback)
{
    m_callbackMap.insert(requestId, CallbackSharedDataPointer(callback.data()));
}

void CallbackDirectory::invoke(quint64 requestId, const QVariant &result)
{
    CallbackSharedDataPointer sharedPtr = m_callbackMap.take(requestId);
    if (sharedPtr) {
        Q_ASSERT(sharedPtr.type == CallbackSharedDataPointer::Variant);
        (*sharedPtr.variantCallback)(result);
    }
}

void CallbackDirectory::invoke(quint64 requestId, const QString &result)
{
    CallbackSharedDataPointer sharedPtr = m_callbackMap.take(requestId);
    if (sharedPtr) {
        Q_ASSERT(sharedPtr.type == CallbackSharedDataPointer::String);
        (*sharedPtr.stringCallback)(result);
    }
}

void CallbackDirectory::invoke(quint64 requestId, bool result)
{
    CallbackSharedDataPointer sharedPtr = m_callbackMap.take(requestId);
    if (sharedPtr) {
        Q_ASSERT(sharedPtr.type == CallbackSharedDataPointer::Bool);
        (*sharedPtr.boolCallback)(result);
    }
}

void CallbackDirectory::CallbackSharedDataPointer::doRef()
{
    switch (type) {
    case None:
        break;
    case Variant:
        variantCallback->ref.ref();
        break;
    case String:
        stringCallback->ref.ref();
        break;
    case Bool:
        boolCallback->ref.ref();
        break;
    }
}

void CallbackDirectory::CallbackSharedDataPointer::doDeref()
{
    switch (type) {
    case None:
        break;
    case Variant:
        if (!variantCallback->ref.deref())
            delete variantCallback;
        break;
    case String:
        if (!stringCallback->ref.deref())
            delete stringCallback;
        break;
    case Bool:
        if (!boolCallback->ref.deref())
            delete boolCallback;
        break;
    }
}

QWebEnginePagePrivate::QWebEnginePagePrivate()
    : adapter(new WebContentsAdapter)
    , history(new QWebEngineHistory(new QWebEngineHistoryPrivate(this)))
    , settings(new QWebEngineSettings)
    , view(0)
    , isLoading(false)
{
    memset(actions, 0, sizeof(actions));
}

QWebEnginePagePrivate::~QWebEnginePagePrivate()
{
    delete history;
    delete settings;
}

RenderWidgetHostViewQtDelegate *QWebEnginePagePrivate::CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client)
{
    return new RenderWidgetHostViewQtDelegateWidget(client);
}

void QWebEnginePagePrivate::titleChanged(const QString &title)
{
    Q_Q(QWebEnginePage);
    Q_EMIT q->titleChanged(title);
}

void QWebEnginePagePrivate::urlChanged(const QUrl &url)
{
    Q_Q(QWebEnginePage);
    explicitUrl = QUrl();
    Q_EMIT q->urlChanged(url);
}

void QWebEnginePagePrivate::iconChanged(const QUrl &url)
{
    Q_Q(QWebEnginePage);
    Q_EMIT q->iconUrlChanged(url);
}

void QWebEnginePagePrivate::loadProgressChanged(int progress)
{
    Q_Q(QWebEnginePage);
    Q_EMIT q->loadProgress(progress);
}

void QWebEnginePagePrivate::didUpdateTargetURL(const QUrl &hoveredUrl)
{
    Q_Q(QWebEnginePage);
    Q_EMIT q->linkHovered(hoveredUrl.toString());
}

void QWebEnginePagePrivate::selectionChanged()
{
    Q_Q(QWebEnginePage);
    Q_EMIT q->selectionChanged();
}

QRectF QWebEnginePagePrivate::viewportRect() const
{
    return view ? view->rect() : QRectF();
}

qreal QWebEnginePagePrivate::dpiScale() const
{
    return 1.0;
}

void QWebEnginePagePrivate::loadStarted(const QUrl &provisionalUrl)
{
    Q_UNUSED(provisionalUrl)
    Q_Q(QWebEnginePage);
    isLoading = true;
    Q_EMIT q->loadStarted();
    updateNavigationActions();
}

void QWebEnginePagePrivate::loadCommitted()
{
    updateNavigationActions();
}

void QWebEnginePagePrivate::loadFinished(bool success, const QUrl &url, int errorCode, const QString &errorDescription)
{
    Q_Q(QWebEnginePage);
    Q_UNUSED(url);
    Q_UNUSED(errorCode);
    Q_UNUSED(errorDescription);
    isLoading = false;
    if (success)
        explicitUrl = QUrl();
    Q_EMIT q->loadFinished(success);
    updateNavigationActions();
}

void QWebEnginePagePrivate::focusContainer()
{
    if (view)
        view->setFocus();
}

void QWebEnginePagePrivate::adoptNewWindow(WebContentsAdapter *newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect &initialGeometry)
{
    Q_Q(QWebEnginePage);
    Q_UNUSED(userGesture);
    QWebEnginePage *newPage = q->createWindow(toWindowType(disposition));
    // Overwrite the new page's WebContents with ours.
    if (newPage && newPage->d_func() != this) {
        newPage->d_func()->adapter = newWebContents;
        newWebContents->initialize(newPage->d_func());
        if (!initialGeometry.isEmpty())
            emit newPage->geometryChangeRequested(initialGeometry);
    }
}

void QWebEnginePagePrivate::close()
{
    Q_Q(QWebEnginePage);
    Q_EMIT q->windowCloseRequested();
}

void QWebEnginePagePrivate::didRunJavaScript(quint64 requestId, const QVariant& result)
{
    m_callbacks.invoke(requestId, result);
}

void QWebEnginePagePrivate::didFetchDocumentMarkup(quint64 requestId, const QString& result)
{
    m_callbacks.invoke(requestId, result);
}

void QWebEnginePagePrivate::didFetchDocumentInnerText(quint64 requestId, const QString& result)
{
    m_callbacks.invoke(requestId, result);
}

void QWebEnginePagePrivate::didFindText(quint64 requestId, int matchCount)
{
    m_callbacks.invoke(requestId, matchCount > 0);
}

void QWebEnginePagePrivate::passOnFocus(bool reverse)
{
    if (view)
        view->focusNextPrevChild(!reverse);
}

void QWebEnginePagePrivate::authenticationRequired(const QUrl &requestUrl, const QString &realm, bool isProxy, const QString &challengingHost, QString *outUser, QString *outPassword)
{
    Q_Q(QWebEnginePage);
    QAuthenticator networkAuth;
    networkAuth.setRealm(realm);

    if (isProxy)
        Q_EMIT q->proxyAuthenticationRequired(requestUrl, &networkAuth, challengingHost);
    else
        Q_EMIT q->authenticationRequired(requestUrl, &networkAuth);
    *outUser = networkAuth.user();
    *outPassword = networkAuth.password();
}

void QWebEnginePagePrivate::runMediaAccessPermissionRequest(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags requestFlags)
{
    Q_Q(QWebEnginePage);
    QWebEnginePage::Feature requestedFeature;
    if (requestFlags.testFlag(WebContentsAdapterClient::MediaAudioCapture) && requestFlags.testFlag(WebContentsAdapterClient::MediaVideoCapture))
        requestedFeature = QWebEnginePage::MediaAudioVideoCapture;
    else if (requestFlags.testFlag(WebContentsAdapterClient::MediaAudioCapture))
        requestedFeature = QWebEnginePage::MediaAudioCapture;
    else if (requestFlags.testFlag(WebContentsAdapterClient::MediaVideoCapture))
        requestedFeature = QWebEnginePage::MediaVideoCapture;
    else
        return;
    Q_EMIT q->featurePermissionRequested(securityOrigin, requestedFeature);
}

QObject *QWebEnginePagePrivate::accessibilityParentObject()
{
    return view;
}

void QWebEnginePagePrivate::updateAction(QWebEnginePage::WebAction action) const
{
#ifdef QT_NO_ACTION
    Q_UNUSED(action)
#else
    QAction *a = actions[action];
    if (!a)
        return;

    bool enabled = true;

    switch (action) {
    case QWebEnginePage::Back:
        enabled = adapter->canGoBack();
        break;
    case QWebEnginePage::Forward:
        enabled = adapter->canGoForward();
        break;
    case QWebEnginePage::Stop:
        enabled = isLoading;
        break;
    case QWebEnginePage::Reload:
    case QWebEnginePage::ReloadAndBypassCache:
        enabled = !isLoading;
        break;
    default:
        break;
    }

    a->setEnabled(enabled);
#endif // QT_NO_ACTION
}

void QWebEnginePagePrivate::updateNavigationActions()
{
    updateAction(QWebEnginePage::Back);
    updateAction(QWebEnginePage::Forward);
    updateAction(QWebEnginePage::Stop);
    updateAction(QWebEnginePage::Reload);
    updateAction(QWebEnginePage::ReloadAndBypassCache);
}

#ifndef QT_NO_ACTION
void QWebEnginePagePrivate::_q_webActionTriggered(bool checked)
{
    Q_Q(QWebEnginePage);
    QAction *a = qobject_cast<QAction *>(q->sender());
    if (!a)
        return;
    QWebEnginePage::WebAction action = static_cast<QWebEnginePage::WebAction>(a->data().toInt());
    q->triggerAction(action, checked);
}
#endif // QT_NO_ACTION

void QWebEnginePagePrivate::recreateFromSerializedHistory(QDataStream &input)
{
    QExplicitlySharedDataPointer<WebContentsAdapter> newWebContents = WebContentsAdapter::createFromSerializedNavigationHistory(input, this);
    if (newWebContents) {
        adapter = newWebContents.data();
        adapter->initialize(this);
    }
}

QWebEnginePage::QWebEnginePage(QObject* parent)
    : QObject(parent)
    , d_ptr(new QWebEnginePagePrivate)
{
    Q_D(QWebEnginePage);
    d->q_ptr = this;
    d->adapter->initialize(d);
}

QWebEnginePage::~QWebEnginePage()
{
    Q_D(QWebEnginePage);
    QWebEngineViewPrivate::bind(d->view, 0);
}

QWebEngineHistory *QWebEnginePage::history() const
{
    Q_D(const QWebEnginePage);
    return d->history;
}

QWebEngineSettings *QWebEnginePage::settings() const
{
    Q_D(const QWebEnginePage);
    return d->settings;
}

void QWebEnginePage::setView(QWidget *view)
{
    QWebEngineViewPrivate::bind(qobject_cast<QWebEngineView*>(view), this);
}

QWidget *QWebEnginePage::view() const
{
    Q_D(const QWebEnginePage);
    return d->view;
}

bool QWebEnginePage::hasSelection() const
{
    return !selectedText().isEmpty();
}

QString QWebEnginePage::selectedText() const
{
    Q_D(const QWebEnginePage);
    return d->adapter->selectedText();
}

#ifndef QT_NO_ACTION
QAction *QWebEnginePage::action(WebAction action) const
{
    Q_D(const QWebEnginePage);
    if (action == QWebEnginePage::NoWebAction)
        return 0;
    if (d->actions[action])
        return d->actions[action];

    QString text;
    QIcon icon;
    QStyle *style = d->view ? d->view->style() : qApp->style();

    switch (action) {
    case Back:
        text = tr("Back");
        icon = style->standardIcon(QStyle::SP_ArrowBack);
        break;
    case Forward:
        text = tr("Forward");
        icon = style->standardIcon(QStyle::SP_ArrowForward);
        break;
    case Stop:
        text = tr("Stop");
        icon = style->standardIcon(QStyle::SP_BrowserStop);
        break;
    case Reload:
        text = tr("Reload");
        icon = style->standardIcon(QStyle::SP_BrowserReload);
        break;
    case Cut:
        text = tr("Cut");
        break;
    case Copy:
        text = tr("Copy");
        break;
    case Paste:
        text = tr("Paste");
        break;
    case Undo:
        text = tr("Undo");
        break;
    case Redo:
        text = tr("Redo");
        break;
    case SelectAll:
        text = tr("Select All");
        break;
    case PasteAndMatchStyle:
        text = tr("Paste and Match Style");
        break;
    default:
        break;
    }

    QAction *a = new QAction(const_cast<QWebEnginePage*>(this));
    a->setText(text);
    a->setData(action);
    a->setIcon(icon);

    connect(a, SIGNAL(triggered(bool)), this, SLOT(_q_webActionTriggered(bool)));

    d->actions[action] = a;
    d->updateAction(action);
    return a;
}
#endif // QT_NO_ACTION

void QWebEnginePage::triggerAction(WebAction action, bool)
{
    Q_D(QWebEnginePage);
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
    default:
        Q_UNREACHABLE();
    }
}

void QWebEnginePage::findText(const QString &subString, FindFlags options, const QWebEngineCallback<bool> &resultCallback)
{
    Q_D(QWebEnginePage);
    if (subString.isEmpty()) {
        d->adapter->stopFinding();
        if (resultCallback.d)
            (*resultCallback.d)(false);
    } else {
        quint64 requestId = d->adapter->findText(subString, options & FindCaseSensitively, options & FindBackward);
        if (resultCallback.d)
            d->m_callbacks.registerCallback(requestId, resultCallback.d);
    }
}

/*!
 * \reimp
 */
bool QWebEnginePage::event(QEvent *e)
{
    return QObject::event(e);
}

bool QWebEnginePagePrivate::contextMenuRequested(const WebEngineContextMenuData &data)
{
    if (!view)
        return false;

    QContextMenuEvent event(QContextMenuEvent::Mouse, data.pos, view->mapToGlobal(data.pos));
    switch (view->contextMenuPolicy()) {
    case Qt::PreventContextMenu:
        return false;
    case Qt::DefaultContextMenu:
        m_menuData = data;
        view->contextMenuEvent(&event);
        break;
    case Qt::CustomContextMenu:
        Q_EMIT view->customContextMenuRequested(data.pos);
        break;
    case Qt::ActionsContextMenu:
        if (view->actions().count()) {
            QMenu::exec(view->actions(), event.globalPos(), 0, view);
            break;
        }
        // fall through
    default:
        event.ignore();
        return false;
        break;
    }
    Q_ASSERT(view->d_func()->m_pendingContextMenuEvent);
    view->d_func()->m_pendingContextMenuEvent = false;
    m_menuData = WebEngineContextMenuData();
    return true;
}

void QWebEnginePagePrivate::javascriptDialog(QSharedPointer<JavaScriptDialogController> controller)
{
    Q_Q(QWebEnginePage);
    bool accepted = false;
    QString promptResult;
    switch (controller->type()) {
    case AlertDialog:
        q->javaScriptAlert(controller->securityOrigin(), controller->message());
        accepted = true;
        break;
    case ConfirmDialog:
        accepted = q->javaScriptConfirm(controller->securityOrigin(), controller->message());
        break;
    case PromptDialog:
        accepted = q->javaScriptPrompt(controller->securityOrigin(), controller->message(), controller->defaultPrompt(), &promptResult);
        if (accepted)
            controller->textProvided(promptResult);
        break;
    case InternalAuthorizationDialog:
        accepted = (QMessageBox::question(view, controller->title(), controller->message(), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes);
        break;
    default:
        Q_UNREACHABLE();
    }
    if (accepted)
        controller->accept();
    else
        controller->reject();
}

void QWebEnginePagePrivate::allowCertificateError(const QExplicitlySharedDataPointer<CertificateErrorController> &controller)
{
    Q_Q(QWebEnginePage);
    bool accepted = false;

    QWebEngineCertificateError error(controller->error(), controller->url(), controller->overridable() && !controller->strictEnforcement(), controller->errorString());
    accepted = q->certificateError(error);

    if (error.isOverridable())
        controller->accept(accepted);
}

void QWebEnginePagePrivate::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID)
{
    Q_Q(QWebEnginePage);
    q->javaScriptConsoleMessage(static_cast<QWebEnginePage::JavaScriptConsoleMessageLevel>(level), message, lineNumber, sourceID);
}

namespace {
class SaveToClipboardFunctor
{
    QString m_text;
public:
    SaveToClipboardFunctor(const QString &text)
      : m_text(text)
    {}
    void operator()() const
    {
        qApp->clipboard()->setText(m_text);
    }
};

class LoadUrlFunctor
{
    QWebEnginePage *m_page;
    QUrl m_url;
public:
    LoadUrlFunctor(QWebEnginePage *page, const QUrl &url)
      : m_page(page)
      , m_url(url)
    {}
    void operator()() const
    {
        m_page->load(m_url);
    }
};
}

QMenu *QWebEnginePage::createStandardContextMenu()
{
    Q_D(QWebEnginePage);
    QMenu *menu = new QMenu(d->view);
    QAction *action = 0;
    WebEngineContextMenuData contextMenuData(d->m_menuData);
    if (contextMenuData.selectedText.isEmpty()) {
        action = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")), tr("&Back"), menu);
        connect(action, &QAction::triggered, d->view, &QWebEngineView::back);
        action->setEnabled(d->adapter->canGoBack());
        menu->addAction(action);

        action = new QAction(QIcon::fromTheme(QStringLiteral("go-next")), tr("&Forward"), menu);
        connect(action, &QAction::triggered, d->view, &QWebEngineView::forward);
        action->setEnabled(d->adapter->canGoForward());
        menu->addAction(action);

        action = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("&Reload"), menu);
        connect(action, &QAction::triggered, d->view, &QWebEngineView::reload);
        menu->addAction(action);
    } else {
        action = new QAction(tr("Copy..."), menu);
        connect(action, &QAction::triggered, SaveToClipboardFunctor(contextMenuData.selectedText));
        menu->addAction(action);
    }

    if (!contextMenuData.linkText.isEmpty() && contextMenuData.linkUrl.isValid()) {
        menu->addSeparator();
        action = new QAction(tr("Navigate to..."), menu);
        connect(action, &QAction::triggered, LoadUrlFunctor(this, contextMenuData.linkUrl));
        menu->addAction(action);
        action = new QAction(tr("Copy link address"), menu);
        connect(action, &QAction::triggered, SaveToClipboardFunctor(contextMenuData.linkUrl.toString()));
        menu->addAction(action);
    }
    return menu;
}

void QWebEnginePage::setFeaturePermission(const QUrl &securityOrigin, QWebEnginePage::Feature feature, QWebEnginePage::PermissionPolicy policy)
{
    Q_D(QWebEnginePage);
    WebContentsAdapterClient::MediaRequestFlags flags =  WebContentsAdapterClient::MediaNone;
    switch (feature) {
    case MediaAudioVideoCapture:
    case MediaAudioCapture:
    case MediaVideoCapture:
        if (policy != PermissionUnknown) {
            if (policy == PermissionDeniedByUser)
                flags = WebContentsAdapterClient::MediaNone;
            else {
                if (feature == MediaAudioCapture)
                    flags = WebContentsAdapterClient::MediaAudioCapture;
                else if (feature == MediaVideoCapture)
                    flags = WebContentsAdapterClient::MediaVideoCapture;
                else
                    flags = WebContentsAdapterClient::MediaRequestFlags(WebContentsAdapterClient::MediaVideoCapture | WebContentsAdapterClient::MediaAudioCapture);
            }
            d->adapter->grantMediaAccessPermission(securityOrigin, flags);
        }
        break;
    default:
        break;
    }
}

static inline QWebEnginePage::FileSelectionMode toPublic(WebContentsAdapterClient::FileChooserMode mode)
{
    // Should the underlying values change, we'll need a switch here.
    return static_cast<QWebEnginePage::FileSelectionMode>(mode);
}

void QWebEnginePagePrivate::runFileChooser(WebContentsAdapterClient::FileChooserMode mode, const QString &defaultFileName, const QStringList &acceptedMimeTypes)
{
    Q_Q(QWebEnginePage);
    QStringList selectedFileNames = q->chooseFiles(toPublic(mode), (QStringList() << defaultFileName), acceptedMimeTypes);
    adapter->filesSelectedInChooser(selectedFileNames, mode);
}

WebEngineSettings *QWebEnginePagePrivate::webEngineSettings() const
{
    return settings->d_func()->coreSettings.data();
}

void QWebEnginePage::load(const QUrl& url)
{
    Q_D(QWebEnginePage);
    d->adapter->load(url);
}

void QWebEnginePage::toHtml(const QWebEngineCallback<const QString &> &resultCallback) const
{
    Q_D(const QWebEnginePage);
    quint64 requestId = d->adapter->fetchDocumentMarkup();
    d->m_callbacks.registerCallback(requestId, resultCallback.d);
}

void QWebEnginePage::toPlainText(const QWebEngineCallback<const QString &> &resultCallback) const
{
    Q_D(const QWebEnginePage);
    quint64 requestId = d->adapter->fetchDocumentInnerText();
    d->m_callbacks.registerCallback(requestId, resultCallback.d);
}

void QWebEnginePage::setHtml(const QString &html, const QUrl &baseUrl)
{
    setContent(html.toUtf8(), QStringLiteral("text/html;charset=UTF-8"), baseUrl);
}

void QWebEnginePage::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    Q_D(QWebEnginePage);
    d->adapter->setContent(data, mimeType, baseUrl);
}

QString QWebEnginePage::title() const
{
    Q_D(const QWebEnginePage);
    return d->adapter->pageTitle();
}

void QWebEnginePage::setUrl(const QUrl &url)
{
    Q_D(QWebEnginePage);
    d->explicitUrl = url;
    load(url);
}

QUrl QWebEnginePage::url() const
{
    Q_D(const QWebEnginePage);
    return d->explicitUrl.isValid() ? d->explicitUrl : d->adapter->activeUrl();
}

QUrl QWebEnginePage::requestedUrl() const
{
    Q_D(const QWebEnginePage);
    return d->adapter->requestedUrl();
}

QUrl QWebEnginePage::iconUrl() const
{
    Q_D(const QWebEnginePage);
    return d->adapter->iconUrl();
}

qreal QWebEnginePage::zoomFactor() const
{
    Q_D(const QWebEnginePage);
    return d->adapter->currentZoomFactor();
}

void QWebEnginePage::setZoomFactor(qreal factor)
{
    Q_D(QWebEnginePage);
    d->adapter->setZoomFactor(factor);
}

void QWebEnginePage::runJavaScript(const QString &scriptSource)
{
    Q_D(QWebEnginePage);
    d->adapter->runJavaScript(scriptSource);
}

void QWebEnginePage::runJavaScript(const QString& scriptSource, const QWebEngineCallback<const QVariant &> &resultCallback)
{
    Q_D(QWebEnginePage);
    quint64 requestId = d->adapter->runJavaScriptCallbackResult(scriptSource);
    d->m_callbacks.registerCallback(requestId, resultCallback.d);
}

QWebEnginePage *QWebEnginePage::createWindow(WebWindowType type)
{
    Q_D(QWebEnginePage);
    if (d->view) {
        QWebEngineView *newView = d->view->createWindow(type);
        if (newView)
            return newView->page();
    }
    return 0;
}

Q_STATIC_ASSERT_X(static_cast<int>(WebContentsAdapterClient::Open) == static_cast<int>(QWebEnginePage::FileSelectOpen), "Enums out of sync");
Q_STATIC_ASSERT_X(static_cast<int>(WebContentsAdapterClient::OpenMultiple) == static_cast<int>(QWebEnginePage::FileSelectOpenMultiple), "Enums out of sync");

QStringList QWebEnginePage::chooseFiles(FileSelectionMode mode, const QStringList &oldFiles, const QStringList &acceptedMimeTypes)
{
    // FIXME: Should we expose this in QWebPage's API ? Right now it is very open and can contain a mix and match of file extensions (which QFileDialog
    // can work with) and mimetypes ranging from text/plain or images/* to application/vnd.openxmlformats-officedocument.spreadsheetml.sheet
    Q_UNUSED(acceptedMimeTypes);
    QStringList ret;
    switch (static_cast<WebContentsAdapterClient::FileChooserMode>(mode)) {
    case WebContentsAdapterClient::OpenMultiple:
        ret = QFileDialog::getOpenFileNames(view(), QString());
        break;
    // Chromium extension, not exposed as part of the public API for now.
    case WebContentsAdapterClient::UploadFolder:
        ret << QFileDialog::getExistingDirectory(view(), tr("Select folder to upload")) + QLatin1Char('/');
        break;
    case WebContentsAdapterClient::Save:
        ret << QFileDialog::getSaveFileName(view(), QString(), (QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + oldFiles.first()));
        break;
    default:
    case WebContentsAdapterClient::Open:
        ret << QFileDialog::getOpenFileName(view(), QString(), oldFiles.first());
        break;
    }
    return ret;
}

void QWebEnginePage::javaScriptAlert(const QUrl &securityOrigin, const QString &msg)
{
    Q_UNUSED(securityOrigin);
    QMessageBox::information(view(), QStringLiteral("Javascript Alert - %1").arg(url().toString()), msg);
}

bool QWebEnginePage::javaScriptConfirm(const QUrl &securityOrigin, const QString &msg)
{
    Q_UNUSED(securityOrigin);
    return (QMessageBox::information(view(), QStringLiteral("Javascript Confirm - %1").arg(url().toString()), msg, QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok);
}

bool QWebEnginePage::javaScriptPrompt(const QUrl &securityOrigin, const QString &msg, const QString &defaultValue, QString *result)
{
    Q_UNUSED(securityOrigin);
    bool ret = false;
    if (result)
        *result = QInputDialog::getText(view(), QStringLiteral("Javascript Prompt - %1").arg(url().toString()), msg, QLineEdit::Normal, defaultValue, &ret);
    return ret;
}

void QWebEnginePage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID)
{
    Q_UNUSED(level);
    Q_UNUSED(message);
    Q_UNUSED(lineNumber);
    Q_UNUSED(sourceID);
}

bool QWebEnginePage::certificateError(const QWebEngineCertificateError &)
{
    return false;
}

QT_END_NAMESPACE

#include "moc_qwebenginepage.cpp"
