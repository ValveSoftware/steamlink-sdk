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

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_contents_adapter.h"
#include "web_contents_adapter_p.h"

#include "browser_accessibility_qt.h"
#include "browser_context_adapter.h"
#include "browser_context_adapter_client.h"
#include "browser_context_qt.h"
#include "download_manager_delegate_qt.h"
#include "media_capture_devices_dispatcher.h"
#include "pdfium_document_wrapper_qt.h"
#include "print_view_manager_qt.h"
#include "qwebenginecallback_p.h"
#include "renderer_host/web_channel_ipc_transport_host.h"
#include "render_view_observer_host_qt.h"
#include "type_conversion.h"
#include "web_contents_adapter_client.h"
#include "web_contents_view_qt.h"
#include "web_engine_context.h"
#include "web_engine_settings.h"

#include <base/run_loop.h>
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_agent_host.h"
#include <content/public/browser/download_manager.h>
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/common/content_constants.h"
#include <content/public/common/drop_data.h>
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

#include <QDir>
#include <QGuiApplication>
#include <QPageLayout>
#include <QStringList>
#include <QStyleHints>
#include <QTimer>
#include <QVariant>
#include <QtCore/qmimedata.h>
#include <QtGui/qaccessible.h>
#include <QtGui/qdrag.h>
#include <QtGui/qpixmap.h>
#include <QtWebChannel/QWebChannel>

namespace QtWebEngineCore {

static const int kTestWindowWidth = 800;
static const int kTestWindowHeight = 600;
static const int kHistoryStreamVersion = 3;

static QVariant fromJSValue(const base::Value *result)
{
    QVariant ret;
    switch (result->GetType()) {
    case base::Value::TYPE_NULL:
        break;
    case base::Value::TYPE_BOOLEAN:
    {
        bool out;
        if (result->GetAsBoolean(&out))
            ret.setValue(out);
        break;
    }
    case base::Value::TYPE_INTEGER:
    {
        int out;
        if (result->GetAsInteger(&out))
            ret.setValue(out);
        break;
    }
    case base::Value::TYPE_DOUBLE:
    {
        double out;
        if (result->GetAsDouble(&out))
            ret.setValue(out);
        break;
    }
    case base::Value::TYPE_STRING:
    {
        base::string16 out;
        if (result->GetAsString(&out))
            ret.setValue(toQt(out));
        break;
    }
    case base::Value::TYPE_LIST:
    {
        const base::ListValue *out;
        if (result->GetAsList(&out)) {
            QVariantList list;
            list.reserve(out->GetSize());
            for (size_t i = 0; i < out->GetSize(); ++i) {
                const base::Value *outVal = 0;
                if (out->Get(i, &outVal) && outVal)
                    list.insert(i, fromJSValue(outVal));
            }
            ret.setValue(list);
        }
        break;
    }
    case base::Value::TYPE_DICTIONARY:
    {
        const base::DictionaryValue *out;
        if (result->GetAsDictionary(&out)) {
            QVariantMap map;
            base::DictionaryValue::Iterator it(*out);
            while (!it.IsAtEnd()) {
                map.insert(toQt(it.key()), fromJSValue(&it.value()));
                it.Advance();
            }
            ret.setValue(map);
        }
        break;
    }
    case base::Value::TYPE_BINARY:
    {
        const base::BinaryValue *out = static_cast<const base::BinaryValue*>(result);
        QByteArray data(out->GetBuffer(), out->GetSize());
        ret.setValue(data);
        break;
    }
    default:
        Q_UNREACHABLE();
        break;
    }
    return ret;
}

static void callbackOnEvaluateJS(WebContentsAdapterClient *adapterClient, quint64 requestId, const base::Value *result)
{
    if (requestId)
        adapterClient->didRunJavaScript(requestId, fromJSValue(result));
}

static void callbackOnPrintingFinished(WebContentsAdapterClient *adapterClient, int requestId, const std::vector<char>& result)
{
    if (requestId)
        adapterClient->didPrintPage(requestId, QByteArray(result.data(), result.size()));
}

static content::WebContents *createBlankWebContents(WebContentsAdapterClient *adapterClient, content::BrowserContext *browserContext)
{
    content::WebContents::CreateParams create_params(browserContext, NULL);
    create_params.routing_id = MSG_ROUTING_NONE;
    create_params.initial_size = gfx::Size(kTestWindowWidth, kTestWindowHeight);
    create_params.context = reinterpret_cast<gfx::NativeView>(adapterClient);
    return content::WebContents::Create(create_params);
}

static void serializeNavigationHistory(const content::NavigationController &controller, QDataStream &output)
{
    const int currentIndex = controller.GetCurrentEntryIndex();
    const int count = controller.GetEntryCount();
    const int pendingIndex = controller.GetPendingEntryIndex();

    output << kHistoryStreamVersion;
    output << count;
    output << currentIndex;

    // Logic taken from SerializedNavigationEntry::WriteToPickle.
    for (int i = 0; i < count; ++i) {
        const content::NavigationEntry* entry = (i == pendingIndex)
            ? controller.GetPendingEntry()
            : controller.GetEntryAtIndex(i);
        if (entry->GetVirtualURL().is_valid()) {
            if (entry->GetHasPostData())
                entry->GetPageState().RemovePasswordData();
            std::string encodedPageState = entry->GetPageState().ToEncodedData();
            output << toQt(entry->GetVirtualURL());
            output << toQt(entry->GetTitle());
            output << QByteArray(encodedPageState.data(), encodedPageState.size());
            output << static_cast<qint32>(entry->GetTransitionType());
            output << entry->GetHasPostData();
            output << toQt(entry->GetReferrer().url);
            output << static_cast<qint32>(entry->GetReferrer().policy);
            output << toQt(entry->GetOriginalRequestURL());
            output << entry->GetIsOverridingUserAgent();
            output << static_cast<qint64>(entry->GetTimestamp().ToInternalValue());
            output << entry->GetHttpStatusCode();
        }
    }
}

static void deserializeNavigationHistory(QDataStream &input, int *currentIndex, std::vector<std::unique_ptr<content::NavigationEntry>> *entries, content::BrowserContext *browserContext)
{
    int version;
    input >> version;
    if (version != kHistoryStreamVersion) {
        // We do not try to decode previous history stream versions.
        // Make sure that our history is cleared and mark the rest of the stream as invalid.
        input.setStatus(QDataStream::ReadCorruptData);
        *currentIndex = -1;
        return;
    }

    int count;
    input >> count >> *currentIndex;

    int pageId = 0;
    entries->reserve(count);
    // Logic taken from SerializedNavigationEntry::ReadFromPickle and ToNavigationEntries.
    for (int i = 0; i < count; ++i) {
        QUrl virtualUrl, referrerUrl, originalRequestUrl;
        QString title;
        QByteArray pageState;
        qint32 transitionType, referrerPolicy;
        bool hasPostData, isOverridingUserAgent;
        qint64 timestamp;
        int httpStatusCode;
        input >> virtualUrl;
        input >> title;
        input >> pageState;
        input >> transitionType;
        input >> hasPostData;
        input >> referrerUrl;
        input >> referrerPolicy;
        input >> originalRequestUrl;
        input >> isOverridingUserAgent;
        input >> timestamp;
        input >> httpStatusCode;

        // If we couldn't unpack the entry successfully, abort everything.
        if (input.status() != QDataStream::Ok) {
            *currentIndex = -1;
            auto it = entries->begin();
            auto end = entries->end();
            for (; it != end; ++it)
                it->reset();
            entries->clear();
            return;
        }

        std::unique_ptr<content::NavigationEntry> entry = content::NavigationController::CreateNavigationEntry(
            toGurl(virtualUrl),
            content::Referrer(toGurl(referrerUrl), static_cast<blink::WebReferrerPolicy>(referrerPolicy)),
            // Use a transition type of reload so that we don't incorrectly
            // increase the typed count.
            ui::PAGE_TRANSITION_RELOAD,
            false,
            // The extra headers are not sync'ed across sessions.
            std::string(),
            browserContext);

        entry->SetTitle(toString16(title));
        entry->SetPageState(content::PageState::CreateFromEncodedData(std::string(pageState.data(), pageState.size())));
        entry->SetPageID(pageId++);
        entry->SetHasPostData(hasPostData);
        entry->SetOriginalRequestURL(toGurl(originalRequestUrl));
        entry->SetIsOverridingUserAgent(isOverridingUserAgent);
        entry->SetTimestamp(base::Time::FromInternalValue(timestamp));
        entry->SetHttpStatusCode(httpStatusCode);
        entries->push_back(std::move(entry));
    }
}

namespace {
static QList<WebContentsAdapter *> recursive_guard_loading_adapters;

class LoadRecursionGuard {
    public:
    static bool isGuarded(WebContentsAdapter *adapter)
    {
        return recursive_guard_loading_adapters.contains(adapter);
    }
    LoadRecursionGuard(WebContentsAdapter *adapter)
        : m_adapter(adapter)
    {
        recursive_guard_loading_adapters.append(adapter);
    }

    ~LoadRecursionGuard() {
        recursive_guard_loading_adapters.removeOne(m_adapter);
    }

    private:
        WebContentsAdapter *m_adapter;
};
} // Anonymous namespace

WebContentsAdapterPrivate::WebContentsAdapterPrivate()
    // This has to be the first thing we create, and the last we destroy.
    : engineContext(WebEngineContext::current())
    , webChannel(0)
    , webChannelWorld(0)
    , adapterClient(0)
    , nextRequestId(CallbackDirectory::ReservedCallbackIdsEnd)
    , lastFindRequestId(0)
    , currentDropAction(Qt::IgnoreAction)
    , inDragUpdateLoop(false)
    , updateDragCursorMessagePollingTimer(new QTimer)
{
}

WebContentsAdapterPrivate::~WebContentsAdapterPrivate()
{
    // Destroy the WebContents first
    webContents.reset();
}

QSharedPointer<WebContentsAdapter> WebContentsAdapter::createFromSerializedNavigationHistory(QDataStream &input, WebContentsAdapterClient *adapterClient)
{
    int currentIndex;
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    deserializeNavigationHistory(input, &currentIndex, &entries, adapterClient->browserContextAdapter()->browserContext());

    if (currentIndex == -1)
        return QSharedPointer<WebContentsAdapter>();

    // Unlike WebCore, Chromium only supports Restoring to a new WebContents instance.
    content::WebContents* newWebContents = createBlankWebContents(adapterClient, adapterClient->browserContextAdapter()->browserContext());
    content::NavigationController &controller = newWebContents->GetController();
    controller.Restore(currentIndex, content::NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY, &entries);

    if (controller.GetActiveEntry()) {
        // Set up the file access rights for the selected navigation entry.
        // TODO(joth): This is duplicated from chrome/.../session_restore.cc and
        // should be shared e.g. in  NavigationController. http://crbug.com/68222
        const int id = newWebContents->GetRenderProcessHost()->GetID();
        const content::PageState& pageState = controller.GetActiveEntry()->GetPageState();
        const std::vector<base::FilePath>& filePaths = pageState.GetReferencedFiles();
        for (std::vector<base::FilePath>::const_iterator file = filePaths.begin(); file != filePaths.end(); ++file)
            content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(id, *file);
    }

    return QSharedPointer<WebContentsAdapter>::create(newWebContents);
}

WebContentsAdapter::WebContentsAdapter(content::WebContents *webContents)
    : d_ptr(new WebContentsAdapterPrivate)
{
    Q_D(WebContentsAdapter);
    d->webContents.reset(webContents);
    initUpdateDragCursorMessagePollingTimer();
}

WebContentsAdapter::~WebContentsAdapter()
{
}

void WebContentsAdapter::initialize(WebContentsAdapterClient *adapterClient)
{
    Q_D(WebContentsAdapter);
    d->adapterClient = adapterClient;
    // We keep a reference to browserContextAdapter to keep it alive as long as we use it.
    // This is needed in case the QML WebEngineProfile is garbage collected before the WebEnginePage.
    d->browserContextAdapter = adapterClient->browserContextAdapter();

    // Create our own if a WebContents wasn't provided at construction.
    if (!d->webContents)
        d->webContents.reset(createBlankWebContents(adapterClient, d->browserContextAdapter->browserContext()));

    // This might replace any adapter that has been initialized with this WebEngineSettings.
    adapterClient->webEngineSettings()->setWebContentsAdapter(this);

    content::RendererPreferences* rendererPrefs = d->webContents->GetMutableRendererPrefs();
    rendererPrefs->use_custom_colors = true;
    // Qt returns a flash time (the whole cycle) in ms, chromium expects just the interval in seconds
    const int qtCursorFlashTime = QGuiApplication::styleHints()->cursorFlashTime();
    rendererPrefs->caret_blink_interval = 0.5 * static_cast<double>(qtCursorFlashTime) / 1000;
    rendererPrefs->user_agent_override = d->browserContextAdapter->httpUserAgent().toStdString();
    rendererPrefs->accept_languages = d->browserContextAdapter->httpAcceptLanguageWithoutQualities().toStdString();
    d->webContents->GetRenderViewHost()->SyncRendererPrefs();

    // Create and attach observers to the WebContents.
    d->webContentsDelegate.reset(new WebContentsDelegateQt(d->webContents.get(), adapterClient));
    d->renderViewObserverHost.reset(new RenderViewObserverHostQt(d->webContents.get(), adapterClient));

    // Let the WebContent's view know about the WebContentsAdapterClient.
    WebContentsViewQt* contentsView = static_cast<WebContentsViewQt*>(static_cast<content::WebContentsImpl*>(d->webContents.get())->GetView());
    contentsView->initialize(adapterClient);

    // This should only be necessary after having restored the history to a new WebContentsAdapter.
    d->webContents->GetController().LoadIfNecessary();

#if defined(ENABLE_BASIC_PRINTING)
    PrintViewManagerQt::CreateForWebContents(webContents());
#endif // defined(ENABLE_BASIC_PRINTING)

    // Create an instance of WebEngineVisitedLinksManager to catch the first
    // content::NOTIFICATION_RENDERER_PROCESS_CREATED event. This event will
    // force to initialize visited links in VisitedLinkSlave.
    // It must be done before creating a RenderView.
    d->browserContextAdapter->visitedLinksManager();

    // Create a RenderView with the initial empty document
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    if (!rvh->IsRenderViewLive())
        static_cast<content::WebContentsImpl*>(d->webContents.get())->CreateRenderViewForRenderManager(rvh, MSG_ROUTING_NONE, MSG_ROUTING_NONE, content::FrameReplicationState());
}

void WebContentsAdapter::reattachRWHV()
{
    Q_D(WebContentsAdapter);
    if (content::RenderWidgetHostView *rwhv = d->webContents->GetRenderWidgetHostView())
        rwhv->InitAsChild(0);
}

bool WebContentsAdapter::canGoBack() const
{
    Q_D(const WebContentsAdapter);
    return d->webContents->GetController().CanGoBack();
}

bool WebContentsAdapter::canGoForward() const
{
    Q_D(const WebContentsAdapter);
    return d->webContents->GetController().CanGoForward();
}

void WebContentsAdapter::stop()
{
    Q_D(WebContentsAdapter);
    content::NavigationController& controller = d->webContents->GetController();

    int index = controller.GetPendingEntryIndex();
    if (index != -1)
        controller.RemoveEntryAtIndex(index);

    d->webContents->Stop();
    focusIfNecessary();
}

void WebContentsAdapter::reload()
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().Reload(/*checkRepost = */false);
    focusIfNecessary();
}

void WebContentsAdapter::reloadAndBypassCache()
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().ReloadBypassingCache(/*checkRepost = */false);
    focusIfNecessary();
}

void WebContentsAdapter::load(const QUrl &url)
{
    // The situation can occur when relying on the editingFinished signal in QML to set the url
    // of the WebView.
    // When enter is pressed, onEditingFinished fires and the url of the webview is set, which
    // calls into this and focuses the webview, taking the focus from the TextField/TextInput,
    // which in turn leads to editingFinished firing again. This scenario would cause a crash
    // down the line when unwinding as the first RenderWidgetHostViewQtDelegateQuick instance is
    // a dangling pointer by that time.

    if (LoadRecursionGuard::isGuarded(this))
        return;
    LoadRecursionGuard guard(this);
    Q_UNUSED(guard);

    Q_D(WebContentsAdapter);
    GURL gurl = toGurl(url);

    // Add URL scheme if missing from view-source URL.
    if (url.scheme() == content::kViewSourceScheme) {
        QUrl pageUrl = QUrl(url.toString().remove(0, strlen(content::kViewSourceScheme) + 1));
        if (pageUrl.scheme().isEmpty()) {
            QUrl extendedUrl = QUrl::fromUserInput(pageUrl.toString());
            extendedUrl = QUrl(QString("%1:%2").arg(content::kViewSourceScheme, extendedUrl.toString()));
            gurl = toGurl(extendedUrl);
        }
    }

    content::NavigationController::LoadURLParams params(gurl);
    params.transition_type = ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.override_user_agent = content::NavigationController::UA_OVERRIDE_TRUE;
    d->webContents->GetController().LoadURLWithParams(params);
    focusIfNecessary();
}

void WebContentsAdapter::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    Q_D(WebContentsAdapter);
    QByteArray encodedData = data.toPercentEncoding();
    std::string urlString("data:");
    urlString.append(mimeType.toStdString());
    urlString.append(",");
    urlString.append(encodedData.constData(), encodedData.length());

    GURL dataUrlToLoad(urlString);
    if (dataUrlToLoad.spec().size() > url::kMaxURLChars) {
        d->adapterClient->loadFinished(false, baseUrl, false, net::ERR_ABORTED);
        return;
    }
    content::NavigationController::LoadURLParams params((dataUrlToLoad));
    params.load_type = content::NavigationController::LOAD_TYPE_DATA;
    params.base_url_for_data_url = toGurl(baseUrl);
    params.virtual_url_for_data_url = baseUrl.isEmpty() ? GURL(url::kAboutBlankURL) : toGurl(baseUrl);
    params.can_load_local_resources = true;
    params.transition_type = ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API);
    params.override_user_agent = content::NavigationController::UA_OVERRIDE_TRUE;
    d->webContents->GetController().LoadURLWithParams(params);
    focusIfNecessary();
    d->webContents->Unselect();
}

void WebContentsAdapter::save(const QString &filePath, int savePageFormat)
{
    Q_D(WebContentsAdapter);
    d->webContentsDelegate->setSavePageInfo(SavePageInfo(filePath, savePageFormat));
    d->webContents->OnSavePage();
}

QUrl WebContentsAdapter::activeUrl() const
{
    Q_D(const WebContentsAdapter);
    return toQt(d->webContents->GetLastCommittedURL());
}

QUrl WebContentsAdapter::requestedUrl() const
{
    Q_D(const WebContentsAdapter);
    content::NavigationEntry* entry = d->webContents->GetController().GetVisibleEntry();
    content::NavigationEntry* pendingEntry = d->webContents->GetController().GetPendingEntry();

    if (entry) {
        if (!entry->GetOriginalRequestURL().is_empty())
            return toQt(entry->GetOriginalRequestURL());

        if (pendingEntry && pendingEntry == entry)
            return toQt(entry->GetURL());
    }
    return QUrl();
}

QUrl WebContentsAdapter::iconUrl() const
{
    Q_D(const WebContentsAdapter);
    if (content::NavigationEntry* entry = d->webContents->GetController().GetVisibleEntry()) {
        content::FaviconStatus favicon = entry->GetFavicon();
        if (favicon.valid)
            return toQt(favicon.url);
    }
    return QUrl();
}

QString WebContentsAdapter::pageTitle() const
{
    Q_D(const WebContentsAdapter);
    return toQt(d->webContents->GetTitle());
}

QString WebContentsAdapter::selectedText() const
{
    Q_D(const WebContentsAdapter);
    return toQt(d->webContents->GetRenderWidgetHostView()->GetSelectedText());
}

void WebContentsAdapter::undo()
{
    Q_D(const WebContentsAdapter);
    d->webContents->Undo();
}

void WebContentsAdapter::redo()
{
    Q_D(const WebContentsAdapter);
    d->webContents->Redo();
}

void WebContentsAdapter::cut()
{
    Q_D(const WebContentsAdapter);
    d->webContents->Cut();
}

void WebContentsAdapter::copy()
{
    Q_D(const WebContentsAdapter);
    d->webContents->Copy();
}

void WebContentsAdapter::paste()
{
    Q_D(const WebContentsAdapter);
    d->webContents->Paste();
}

void WebContentsAdapter::pasteAndMatchStyle()
{
    Q_D(const WebContentsAdapter);
    d->webContents->PasteAndMatchStyle();
}

void WebContentsAdapter::selectAll()
{
    Q_D(const WebContentsAdapter);
    d->webContents->SelectAll();
}

void WebContentsAdapter::requestClose()
{
    Q_D(WebContentsAdapter);
    d->webContents->DispatchBeforeUnload();
}

void WebContentsAdapter::unselect()
{
    Q_D(const WebContentsAdapter);
    d->webContents->Unselect();
}

void WebContentsAdapter::navigateToIndex(int offset)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().GoToIndex(offset);
    focusIfNecessary();
}

void WebContentsAdapter::navigateToOffset(int offset)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().GoToOffset(offset);
    focusIfNecessary();
}

int WebContentsAdapter::navigationEntryCount()
{
    Q_D(WebContentsAdapter);
    return d->webContents->GetController().GetEntryCount();
}

int WebContentsAdapter::currentNavigationEntryIndex()
{
    Q_D(WebContentsAdapter);
    return d->webContents->GetController().GetCurrentEntryIndex();
}

QUrl WebContentsAdapter::getNavigationEntryOriginalUrl(int index)
{
    Q_D(WebContentsAdapter);
    content::NavigationEntry *entry = d->webContents->GetController().GetEntryAtIndex(index);
    return entry ? toQt(entry->GetOriginalRequestURL()) : QUrl();
}

QUrl WebContentsAdapter::getNavigationEntryUrl(int index)
{
    Q_D(WebContentsAdapter);
    content::NavigationEntry *entry = d->webContents->GetController().GetEntryAtIndex(index);
    return entry ? toQt(entry->GetURL()) : QUrl();
}

QString WebContentsAdapter::getNavigationEntryTitle(int index)
{
    Q_D(WebContentsAdapter);
    content::NavigationEntry *entry = d->webContents->GetController().GetEntryAtIndex(index);
    return entry ? toQt(entry->GetTitle()) : QString();
}

QDateTime WebContentsAdapter::getNavigationEntryTimestamp(int index)
{
    Q_D(WebContentsAdapter);
    content::NavigationEntry *entry = d->webContents->GetController().GetEntryAtIndex(index);
    return entry ? toQt(entry->GetTimestamp()) : QDateTime();
}

QUrl WebContentsAdapter::getNavigationEntryIconUrl(int index)
{
    Q_D(WebContentsAdapter);
    content::NavigationEntry *entry = d->webContents->GetController().GetEntryAtIndex(index);
    if (!entry)
        return QUrl();
    content::FaviconStatus favicon = entry->GetFavicon();
    return favicon.valid ? toQt(favicon.url) : QUrl();
}

void WebContentsAdapter::clearNavigationHistory()
{
    Q_D(WebContentsAdapter);
    if (d->webContents->GetController().CanPruneAllButLastCommitted())
        d->webContents->GetController().PruneAllButLastCommitted();
}

void WebContentsAdapter::serializeNavigationHistory(QDataStream &output)
{
    Q_D(WebContentsAdapter);
    QtWebEngineCore::serializeNavigationHistory(d->webContents->GetController(), output);
}

void WebContentsAdapter::setZoomFactor(qreal factor)
{
    Q_D(WebContentsAdapter);
    if (factor < content::kMinimumZoomFactor || factor > content::kMaximumZoomFactor)
        return;

    double zoomLevel = content::ZoomFactorToZoomLevel(static_cast<double>(factor));
    content::HostZoomMap *zoomMap = content::HostZoomMap::GetForWebContents(d->webContents.get());

    if (zoomMap) {
        int render_process_id = d->webContents->GetRenderProcessHost()->GetID();
        int render_view_id = d->webContents->GetRenderViewHost()->GetRoutingID();
        zoomMap->SetTemporaryZoomLevel(render_process_id, render_view_id, zoomLevel);
    }
}

qreal WebContentsAdapter::currentZoomFactor() const
{
    Q_D(const WebContentsAdapter);
    return content::ZoomLevelToZoomFactor(content::HostZoomMap::GetZoomLevel(d->webContents.get()));
}

BrowserContextQt* WebContentsAdapter::browserContext()
{
    Q_D(WebContentsAdapter);
    return d->browserContextAdapter ? d->browserContextAdapter->browserContext() : d->webContents ? static_cast<BrowserContextQt*>(d->webContents->GetBrowserContext()) : 0;
}

BrowserContextAdapter* WebContentsAdapter::browserContextAdapter()
{
    Q_D(WebContentsAdapter);
    return d->browserContextAdapter ? d->browserContextAdapter.data() : d->webContents ? static_cast<BrowserContextQt*>(d->webContents->GetBrowserContext())->adapter() : 0;
}

#ifndef QT_NO_ACCESSIBILITY
QAccessibleInterface *WebContentsAdapter::browserAccessible()
{
    Q_D(const WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    content::BrowserAccessibilityManager *manager = static_cast<content::RenderFrameHostImpl*>(rvh->GetMainFrame())->GetOrCreateBrowserAccessibilityManager();
    content::BrowserAccessibility *acc = manager->GetRoot();
    content::BrowserAccessibilityQt *accQt = static_cast<content::BrowserAccessibilityQt*>(acc);
    return accQt;
}
#endif // QT_NO_ACCESSIBILITY

void WebContentsAdapter::runJavaScript(const QString &javaScript, quint32 worldId)
{
    Q_D(WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    if (worldId == 0) {
        rvh->GetMainFrame()->ExecuteJavaScript(toString16(javaScript));
        return;
    }

    content::RenderFrameHost::JavaScriptResultCallback callback = base::Bind(&callbackOnEvaluateJS, d->adapterClient, CallbackDirectory::NoCallbackId);
    rvh->GetMainFrame()->ExecuteJavaScriptInIsolatedWorld(toString16(javaScript), callback, worldId);
}

quint64 WebContentsAdapter::runJavaScriptCallbackResult(const QString &javaScript, quint32 worldId)
{
    Q_D(WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    content::RenderFrameHost::JavaScriptResultCallback callback = base::Bind(&callbackOnEvaluateJS, d->adapterClient, d->nextRequestId);
    if (worldId == 0)
        rvh->GetMainFrame()->ExecuteJavaScript(toString16(javaScript), callback);
    else
        rvh->GetMainFrame()->ExecuteJavaScriptInIsolatedWorld(toString16(javaScript), callback, worldId);
    return d->nextRequestId++;
}

quint64 WebContentsAdapter::fetchDocumentMarkup()
{
    Q_D(WebContentsAdapter);
    d->renderViewObserverHost->fetchDocumentMarkup(d->nextRequestId);
    return d->nextRequestId++;
}

quint64 WebContentsAdapter::fetchDocumentInnerText()
{
    Q_D(WebContentsAdapter);
    d->renderViewObserverHost->fetchDocumentInnerText(d->nextRequestId);
    return d->nextRequestId++;
}

quint64 WebContentsAdapter::findText(const QString &subString, bool caseSensitively, bool findBackward)
{
    Q_D(WebContentsAdapter);
    if (d->lastFindRequestId > d->webContentsDelegate->lastReceivedFindReply()) {
        // There are cases where the render process will overwrite a previous request
        // with the new search and we'll have a dangling callback, leaving the application
        // waiting for it forever.
        // Assume that any unfinished find has been unsuccessful when a new one is started
        // to cover that case.
        d->adapterClient->didFindText(d->lastFindRequestId, 0);
    }

    blink::WebFindOptions options;
    options.forward = !findBackward;
    options.matchCase = caseSensitively;
    options.findNext = subString == d->webContentsDelegate->lastSearchedString();
    d->webContentsDelegate->setLastSearchedString(subString);

    // Find already allows a request ID as input, but only as an int.
    // Use the same counter but mod it to MAX_INT, this keeps the same likeliness of request ID clashing.
    int shrunkRequestId = d->nextRequestId++ & 0x7fffffff;
    d->webContents->Find(shrunkRequestId, toString16(subString), options);
    d->lastFindRequestId = shrunkRequestId;
    return shrunkRequestId;
}

void WebContentsAdapter::stopFinding()
{
    Q_D(WebContentsAdapter);
    d->webContentsDelegate->setLastSearchedString(QString());
    // Clear any previous selection,
    // but keep the renderer blue rectangle selection just like Chromium does.
    d->webContents->Unselect();
    d->webContents->StopFinding(content::STOP_FIND_ACTION_KEEP_SELECTION);
}

void WebContentsAdapter::updateWebPreferences(const content::WebPreferences & webPreferences)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetRenderViewHost()->UpdateWebkitPreferences(webPreferences);
}

void WebContentsAdapter::download(const QUrl &url, const QString &suggestedFileName)
{
    Q_D(WebContentsAdapter);
    content::BrowserContext *bctx = webContents()->GetBrowserContext();
    content::DownloadManager *dlm =  content::BrowserContext::GetDownloadManager(bctx);
    DownloadManagerDelegateQt *dlmd = d->browserContextAdapter->downloadManagerDelegate();

    if (!dlm)
        return;

    dlmd->setDownloadType(BrowserContextAdapterClient::UserRequested);
    dlm->SetDelegate(dlmd);

    std::unique_ptr<content::DownloadUrlParameters> params(
            content::DownloadUrlParameters::CreateForWebContentsMainFrame(webContents(), toGurl(url)));
    params->set_suggested_name(toString16(suggestedFileName));
    dlm->DownloadUrl(std::move(params));
}

bool WebContentsAdapter::isAudioMuted() const
{
    const Q_D(WebContentsAdapter);
    return d->webContents->IsAudioMuted();
}

void WebContentsAdapter::setAudioMuted(bool muted)
{
    Q_D(WebContentsAdapter);
    d->webContents->SetAudioMuted(muted);
}

bool WebContentsAdapter::recentlyAudible()
{
    Q_D(WebContentsAdapter);
    return d->webContents->WasRecentlyAudible();
}

void WebContentsAdapter::copyImageAt(const QPoint &location)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetRenderViewHost()->GetMainFrame()->CopyImageAt(location.x(), location.y());
}

ASSERT_ENUMS_MATCH(WebContentsAdapter::MediaPlayerNoAction, blink::WebMediaPlayerAction::Unknown)
ASSERT_ENUMS_MATCH(WebContentsAdapter::MediaPlayerPlay, blink::WebMediaPlayerAction::Play)
ASSERT_ENUMS_MATCH(WebContentsAdapter::MediaPlayerMute, blink::WebMediaPlayerAction::Mute)
ASSERT_ENUMS_MATCH(WebContentsAdapter::MediaPlayerLoop,  blink::WebMediaPlayerAction::Loop)
ASSERT_ENUMS_MATCH(WebContentsAdapter::MediaPlayerControls,  blink::WebMediaPlayerAction::Controls)

void WebContentsAdapter::executeMediaPlayerActionAt(const QPoint &location, MediaPlayerAction action, bool enable)
{
    Q_D(WebContentsAdapter);
    blink::WebMediaPlayerAction blinkAction((blink::WebMediaPlayerAction::Type)action, enable);
    d->webContents->GetRenderViewHost()->ExecuteMediaPlayerActionAtLocation(toGfx(location), blinkAction);
}

void WebContentsAdapter::inspectElementAt(const QPoint &location)
{
    Q_D(WebContentsAdapter);
    if (content::DevToolsAgentHost::HasFor(d->webContents.get())) {
        content::DevToolsAgentHost::GetOrCreateFor(d->webContents.get())->InspectElement(location.x(), location.y());
    }
}

bool WebContentsAdapter::hasInspector() const
{
    const Q_D(WebContentsAdapter);
    if (content::DevToolsAgentHost::HasFor(d->webContents.get()))
        return content::DevToolsAgentHost::GetOrCreateFor(d->webContents.get())->IsAttached();
    return false;
}

void WebContentsAdapter::exitFullScreen()
{
    Q_D(WebContentsAdapter);
    d->webContents->ExitFullscreen(false);
}

void WebContentsAdapter::changedFullScreen()
{
    Q_D(WebContentsAdapter);
    d->webContents->NotifyFullscreenChanged(false);
}

void WebContentsAdapter::wasShown()
{
    Q_D(WebContentsAdapter);
    d->webContents->WasShown();
}

void WebContentsAdapter::wasHidden()
{
    Q_D(WebContentsAdapter);
    d->webContents->WasHidden();
}

void WebContentsAdapter::printToPDF(const QPageLayout &pageLayout, const QString &filePath)
{
#if defined(ENABLE_BASIC_PRINTING)
    PrintViewManagerQt::FromWebContents(webContents())->PrintToPDF(pageLayout, true, filePath);
#endif // if defined(ENABLE_BASIC_PRINTING)
}

quint64 WebContentsAdapter::printToPDFCallbackResult(const QPageLayout &pageLayout, const bool colorMode)
{
#if defined(ENABLE_BASIC_PRINTING)
    Q_D(WebContentsAdapter);
    PrintViewManagerQt::PrintToPDFCallback callback = base::Bind(&callbackOnPrintingFinished
                                                                 , d->adapterClient
                                                                 , d->nextRequestId);
    PrintViewManagerQt::FromWebContents(webContents())->PrintToPDFWithCallback(pageLayout, colorMode
                                                                               , callback);
    return d->nextRequestId++;
#else
    return 0;
#endif // if defined(ENABLE_BASIC_PRINTING)
}

QPointF WebContentsAdapter::lastScrollOffset() const
{
    Q_D(const WebContentsAdapter);
    if (content::RenderWidgetHostView *rwhv = d->webContents->GetRenderWidgetHostView())
        return toQt(rwhv->GetLastScrollOffset());
    return QPointF();
}

QSizeF WebContentsAdapter::lastContentsSize() const
{
    Q_D(const WebContentsAdapter);
    if (RenderWidgetHostViewQt *rwhv = static_cast<RenderWidgetHostViewQt *>(d->webContents->GetRenderWidgetHostView()))
        return toQt(rwhv->lastContentsSize());
    return QSizeF();
}

void WebContentsAdapter::grantMediaAccessPermission(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags flags)
{
    Q_D(WebContentsAdapter);
    // Let the permission manager remember the reply.
    if (flags & WebContentsAdapterClient::MediaAudioCapture)
        d->browserContextAdapter->permissionRequestReply(securityOrigin, BrowserContextAdapter::AudioCapturePermission, true);
    if (flags & WebContentsAdapterClient::MediaVideoCapture)
        d->browserContextAdapter->permissionRequestReply(securityOrigin, BrowserContextAdapter::VideoCapturePermission, true);
    MediaCaptureDevicesDispatcher::GetInstance()->handleMediaAccessPermissionResponse(d->webContents.get(), securityOrigin, flags);
}

void WebContentsAdapter::runGeolocationRequestCallback(const QUrl &securityOrigin, bool allowed)
{
    Q_D(WebContentsAdapter);
    d->browserContextAdapter->permissionRequestReply(securityOrigin, BrowserContextAdapter::GeolocationPermission, allowed);
}

void WebContentsAdapter::grantMouseLockPermission(bool granted)
{
    Q_D(WebContentsAdapter);

    if (granted) {
        if (RenderWidgetHostViewQt *rwhv = static_cast<RenderWidgetHostViewQt *>(d->webContents->GetRenderWidgetHostView()))
            rwhv->Focus();
        else
            granted = false;
    }

    d->webContents->GotResponseToLockMouseRequest(granted);
}

void WebContentsAdapter::dpiScaleChanged()
{
    Q_D(WebContentsAdapter);
    content::RenderWidgetHostImpl* impl = NULL;
    if (d->webContents->GetRenderViewHost())
        impl = content::RenderWidgetHostImpl::From(d->webContents->GetRenderViewHost()->GetWidget());
    if (impl)
        impl->NotifyScreenInfoChanged();
}

void WebContentsAdapter::backgroundColorChanged()
{
    Q_D(WebContentsAdapter);
    if (content::RenderWidgetHostView *rwhv = d->webContents->GetRenderWidgetHostView())
        rwhv->SetBackgroundColor(toSk(d->adapterClient->backgroundColor()));
}

content::WebContents *WebContentsAdapter::webContents() const
{
    Q_D(const WebContentsAdapter);
    return d->webContents.get();
}

QWebChannel *WebContentsAdapter::webChannel() const
{
    Q_D(const WebContentsAdapter);
    return d->webChannel;
}

void WebContentsAdapter::setWebChannel(QWebChannel *channel, uint worldId)
{
    Q_D(WebContentsAdapter);
    if (d->webChannel == channel && d->webChannelWorld == worldId)
        return;

    if (!d->webChannelTransport.get())
        d->webChannelTransport.reset(new WebChannelIPCTransportHost(d->webContents.get(), worldId));
    else {
        if (d->webChannel != channel)
            d->webChannel->disconnectFrom(d->webChannelTransport.get());
        if (d->webChannelWorld != worldId)
            d->webChannelTransport->setWorldId(worldId);
    }

    d->webChannel = channel;
    d->webChannelWorld = worldId;
    if (!channel) {
        d->webChannelTransport.reset();
        return;
    }
    channel->connectTo(d->webChannelTransport.get());
}

static QMimeData *mimeDataFromDropData(const content::DropData &dropData)
{
    QMimeData *mimeData = new QMimeData();
    if (!dropData.text.is_null())
        mimeData->setText(toQt(dropData.text.string()));
    if (!dropData.html.is_null())
        mimeData->setHtml(toQt(dropData.html.string()));
    if (dropData.url.is_valid())
        mimeData->setUrls(QList<QUrl>() << toQt(dropData.url));
    return mimeData;
}

void WebContentsAdapter::startDragging(QObject *dragSource, const content::DropData &dropData,
                                       Qt::DropActions allowedActions, const QPixmap &pixmap,
                                       const QPoint &offset)
{
    Q_D(WebContentsAdapter);

    if (d->currentDropData)
        return;

    // Clear certain fields of the drop data to not run into DCHECKs
    // of DropDataToWebDragData in render_view_impl.cc.
    d->currentDropData.reset(new content::DropData(dropData));
    d->currentDropData->download_metadata.clear();
    d->currentDropData->file_contents.clear();
    d->currentDropData->file_description_filename.clear();

    d->currentDropAction = Qt::IgnoreAction;
    QDrag *drag = new QDrag(dragSource);    // will be deleted by Qt's DnD implementation
    drag->setMimeData(mimeDataFromDropData(*d->currentDropData));
    if (!pixmap.isNull()) {
        drag->setPixmap(pixmap);
        drag->setHotSpot(offset);
    }

    {
        base::MessageLoop::ScopedNestableTaskAllower allow(base::MessageLoop::current());
        drag->exec(allowedActions);
    }

    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    rvh->DragSourceSystemDragEnded();
    d->currentDropData.reset();
}

static blink::WebDragOperationsMask toWeb(const Qt::DropActions action)
{
    int result = blink::WebDragOperationNone;
    if (action & Qt::CopyAction)
        result |= blink::WebDragOperationCopy;
    if (action & Qt::LinkAction)
        result |= blink::WebDragOperationLink;
    if (action & Qt::MoveAction)
        result |= blink::WebDragOperationMove;
    return static_cast<blink::WebDragOperationsMask>(result);
}

static void fillDropDataFromMimeData(content::DropData *dropData, const QMimeData *mimeData)
{
    Q_ASSERT(dropData->filenames.empty());
    Q_FOREACH (const QUrl &url, mimeData->urls()) {
        if (url.isLocalFile()) {
            ui::FileInfo uifi;
            uifi.path = toFilePath(url.toLocalFile());
            dropData->filenames.push_back(uifi);
        }
    }
    if (!dropData->filenames.empty())
        return;
    if (mimeData->hasHtml())
        dropData->html = toNullableString16(mimeData->html());
    else if (mimeData->hasText())
        dropData->text = toNullableString16(mimeData->text());
}

void WebContentsAdapter::enterDrag(QDragEnterEvent *e, const QPoint &screenPos)
{
    Q_D(WebContentsAdapter);

    if (!d->currentDropData) {
        // The drag originated outside the WebEngineView.
        d->currentDropData.reset(new content::DropData);
        fillDropDataFromMimeData(d->currentDropData.get(), e->mimeData());
    }

    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    rvh->FilterDropData(d->currentDropData.get());
    rvh->DragTargetDragEnter(*d->currentDropData, toGfx(e->pos()), toGfx(screenPos),
                             toWeb(e->possibleActions()),
                             flagsFromModifiers(e->keyboardModifiers()));
}

Qt::DropAction WebContentsAdapter::updateDragPosition(QDragMoveEvent *e, const QPoint &screenPos)
{
    Q_D(WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    rvh->DragTargetDragOver(toGfx(e->pos()), toGfx(screenPos), toWeb(e->possibleActions()),
                            blink::WebInputEvent::LeftButtonDown);

    base::MessageLoop *currentMessageLoop = base::MessageLoop::current();
    DCHECK(currentMessageLoop);
    if (!currentMessageLoop->NestableTasksAllowed()) {
        // We're already inside a MessageLoop::RunTask call, and scheduled tasks will not be
        // executed. That means, updateDragAction will never be called, and the RunLoop below will
        // remain blocked forever.
        qWarning("WebContentsAdapter::updateDragPosition called from MessageLoop::RunTask.");
        return Qt::IgnoreAction;
    }

    // Wait until we get notified via RenderViewHostDelegateView::UpdateDragCursor. This calls
    // WebContentsAdapter::updateDragAction that will eventually quit the nested loop.
    base::RunLoop loop;
    d->inDragUpdateLoop = true;
    d->dragUpdateLoopQuitClosure = loop.QuitClosure();

    d->updateDragCursorMessagePollingTimer->start();
    loop.Run();
    d->updateDragCursorMessagePollingTimer->stop();

    return d->currentDropAction;
}

void WebContentsAdapter::updateDragAction(Qt::DropAction action)
{
    Q_D(WebContentsAdapter);
    d->currentDropAction = action;
    finishDragUpdate();
}

void WebContentsAdapter::finishDragUpdate()
{
    Q_D(WebContentsAdapter);
    if (d->inDragUpdateLoop) {
        d->dragUpdateLoopQuitClosure.Run();
        d->inDragUpdateLoop = false;
    }
}

void WebContentsAdapter::endDragging(const QPoint &clientPos, const QPoint &screenPos)
{
    Q_D(WebContentsAdapter);
    finishDragUpdate();
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    rvh->FilterDropData(d->currentDropData.get());
    rvh->DragTargetDrop(*d->currentDropData, toGfx(clientPos), toGfx(screenPos), 0);
    d->currentDropData.reset();
}

void WebContentsAdapter::leaveDrag()
{
    Q_D(WebContentsAdapter);
    finishDragUpdate();
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    rvh->DragTargetDragLeave();
    d->currentDropData.reset();
}

void WebContentsAdapter::initUpdateDragCursorMessagePollingTimer()
{
    Q_D(WebContentsAdapter);
    // Poll for drag cursor updated message 60 times per second. In practice, the timer is fired
    // at most twice, after which it is stopped.
    d->updateDragCursorMessagePollingTimer->setInterval(16);
    d->updateDragCursorMessagePollingTimer->setSingleShot(false);

    QObject::connect(d->updateDragCursorMessagePollingTimer.data(), &QTimer::timeout, [](){
        base::MessagePump::Delegate *delegate = base::MessageLoop::current();
        DCHECK(delegate);

        // Execute Chromium tasks if there are any present. Specifically we are interested to handle
        // the RenderViewHostImpl::OnUpdateDragCursor message, that gets sent from the render
        // process.
        while (delegate->DoWork()) {}
    });
}


void WebContentsAdapter::replaceMisspelling(const QString &word)
{
#if defined(ENABLE_SPELLCHECK)
    Q_D(WebContentsAdapter);
    d->webContents->ReplaceMisspelling(toString16(word));
#endif
}

void WebContentsAdapter::focusIfNecessary()
{
    Q_D(WebContentsAdapter);
    const WebEngineSettings *settings = d->adapterClient->webEngineSettings();
    bool focusOnNavigation = settings->testAttribute(WebEngineSettings::FocusOnNavigationEnabled);
    if (focusOnNavigation)
        d->webContents->Focus();
}

WebContentsAdapterClient::RenderProcessTerminationStatus
WebContentsAdapterClient::renderProcessExitStatus(int terminationStatus) {
    auto status = WebContentsAdapterClient::RenderProcessTerminationStatus(-1);
    switch (terminationStatus) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
        status = WebContentsAdapterClient::NormalTerminationStatus;
        break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
        status = WebContentsAdapterClient::AbnormalTerminationStatus;
        break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
        status = WebContentsAdapterClient::KilledTerminationStatus;
        break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
        status = WebContentsAdapterClient::CrashedTerminationStatus;
        break;
    case base::TERMINATION_STATUS_STILL_RUNNING:
    case base::TERMINATION_STATUS_MAX_ENUM:
        // should be unreachable since Chromium asserts status != TERMINATION_STATUS_STILL_RUNNING
        // before calling this method
        break;
    }

    return status;
}

FaviconManager *WebContentsAdapter::faviconManager()
{
    Q_D(WebContentsAdapter);
    return d->webContentsDelegate->faviconManager();
}

void WebContentsAdapter::viewSource()
{
    Q_D(WebContentsAdapter);
    d->webContents->ViewSource();
}

bool WebContentsAdapter::canViewSource()
{
    Q_D(WebContentsAdapter);
    return d->webContents->GetController().CanViewSource();
}

} // namespace QtWebEngineCore
