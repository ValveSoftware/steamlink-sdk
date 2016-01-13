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

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_contents_adapter.h"
#include "web_contents_adapter_p.h"

#include "browser_context_qt.h"
#include "content_browser_client_qt.h"
#include "javascript_dialog_manager_qt.h"
#include "media_capture_devices_dispatcher.h"
#include "qt_render_view_observer_host.h"
#include "type_conversion.h"
#include "web_contents_adapter_client.h"
#include "web_contents_view_qt.h"
#include "web_engine_context.h"
#include "web_engine_settings.h"
#include "web_engine_visited_links_manager.h"

#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "webkit/common/webpreferences.h"

#include <QDir>
#include <QGuiApplication>
#include <QStringList>
#include <QStyleHints>
#include <QVariant>
#include <QtGui/qaccessible.h>

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
    adapterClient->didRunJavaScript(requestId, fromJSValue(result));
}

static QStringList listRecursively(const QDir& dir) {
    QStringList ret;
    QFileInfoList infoList(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot |QDir::Hidden));
    Q_FOREACH (const QFileInfo &fileInfo, infoList) {
        if (fileInfo.isDir()) {
            ret.append(fileInfo.absolutePath() + QStringLiteral("/.")); // Match chromium's behavior. See chrome/browser/file_select_helper.cc
            ret.append(listRecursively(QDir(fileInfo.absoluteFilePath())));
        } else
            ret.append(fileInfo.absoluteFilePath());
    }
    return ret;
}

static content::WebContents *createBlankWebContents(WebContentsAdapterClient *adapterClient)
{
    content::BrowserContext* browserContext = ContentBrowserClientQt::Get()->browser_context();
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
            // If you want to navigate a named frame in Chrome, you will first need to
            // add support for persisting it. It is currently only used for layout tests.
            CHECK(entry->GetFrameToNavigate().empty());
        }
    }
}

void deserializeNavigationHistory(QDataStream &input, int *currentIndex, std::vector<content::NavigationEntry*> *entries)
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
            Q_FOREACH (content::NavigationEntry *entry, *entries)
                delete entry;
            entries->clear();
            return;
        }

        content::NavigationEntry *entry = content::NavigationController::CreateNavigationEntry(
            toGurl(virtualUrl),
            content::Referrer(toGurl(referrerUrl), static_cast<blink::WebReferrerPolicy>(referrerPolicy)),
            // Use a transition type of reload so that we don't incorrectly
            // increase the typed count.
            content::PAGE_TRANSITION_RELOAD,
            false,
            // The extra headers are not sync'ed across sessions.
            std::string(),
            ContentBrowserClientQt::Get()->browser_context());

        entry->SetTitle(toString16(title));
        entry->SetPageState(content::PageState::CreateFromEncodedData(std::string(pageState.data(), pageState.size())));
        entry->SetPageID(pageId++);
        entry->SetHasPostData(hasPostData);
        entry->SetOriginalRequestURL(toGurl(originalRequestUrl));
        entry->SetIsOverridingUserAgent(isOverridingUserAgent);
        entry->SetTimestamp(base::Time::FromInternalValue(timestamp));
        entry->SetHttpStatusCode(httpStatusCode);
        entries->push_back(entry);
    }
}

WebContentsAdapterPrivate::WebContentsAdapterPrivate()
    // This has to be the first thing we create, and the last we destroy.
    : engineContext(WebEngineContext::current())
    , nextRequestId(1)
    , lastFindRequestId(0)
{
}

WebContentsAdapterPrivate::~WebContentsAdapterPrivate()
{
}

QExplicitlySharedDataPointer<WebContentsAdapter> WebContentsAdapter::createFromSerializedNavigationHistory(QDataStream &input, WebContentsAdapterClient *adapterClient)
{
    int currentIndex;
    std::vector<content::NavigationEntry*> entries;
    deserializeNavigationHistory(input, &currentIndex, &entries);

    if (currentIndex == -1)
        return QExplicitlySharedDataPointer<WebContentsAdapter>();

    // Unlike WebCore, Chromium only supports Restoring to a new WebContents instance.
    content::WebContents* newWebContents = createBlankWebContents(adapterClient);
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

    return QExplicitlySharedDataPointer<WebContentsAdapter>(new WebContentsAdapter(newWebContents));
}

WebContentsAdapter::WebContentsAdapter(content::WebContents *webContents)
    : d_ptr(new WebContentsAdapterPrivate)
{
    Q_D(WebContentsAdapter);
    d->webContents.reset(webContents);
}

WebContentsAdapter::~WebContentsAdapter()
{
}

void WebContentsAdapter::initialize(WebContentsAdapterClient *adapterClient)
{
    Q_D(WebContentsAdapter);
    d->adapterClient = adapterClient;

    // Create our own if a WebContents wasn't provided at construction.
    if (!d->webContents)
        d->webContents.reset(createBlankWebContents(adapterClient));

    // This might replace any adapter that has been initialized with this WebEngineSettings.
    adapterClient->webEngineSettings()->setWebContentsAdapter(this);

    content::RendererPreferences* rendererPrefs = d->webContents->GetMutableRendererPrefs();
    rendererPrefs->use_custom_colors = true;
    // Qt returns a flash time (the whole cycle) in ms, chromium expects just the interval in seconds
    const int qtCursorFlashTime = QGuiApplication::styleHints()->cursorFlashTime();
    rendererPrefs->caret_blink_interval = 0.5 * static_cast<double>(qtCursorFlashTime) / 1000;
    d->webContents->GetRenderViewHost()->SyncRendererPrefs();

    // Create and attach observers to the WebContents.
    d->webContentsDelegate.reset(new WebContentsDelegateQt(d->webContents.get(), adapterClient));
    d->renderViewObserverHost.reset(new QtRenderViewObserverHost(d->webContents.get(), adapterClient));

    // Let the WebContent's view know about the WebContentsAdapterClient.
    WebContentsViewQt* contentsView = static_cast<WebContentsViewQt*>(static_cast<content::WebContentsImpl*>(d->webContents.get())->GetView());
    contentsView->initialize(adapterClient);

    // This should only be necessary after having restored the history to a new WebContentsAdapter.
    d->webContents->GetController().LoadIfNecessary();

    // Create a RenderView with the initial empty document
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    if (!rvh->IsRenderViewLive())
        static_cast<content::WebContentsImpl*>(d->webContents.get())->CreateRenderViewForRenderManager(rvh, MSG_ROUTING_NONE, MSG_ROUTING_NONE, true);
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
    d->webContents->Focus();
}

void WebContentsAdapter::reload()
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().Reload(/*checkRepost = */false);
    d->webContents->Focus();
}

void WebContentsAdapter::load(const QUrl &url)
{
    Q_D(WebContentsAdapter);
    content::NavigationController::LoadURLParams params(toGurl(url));
    params.transition_type = content::PageTransitionFromInt(content::PAGE_TRANSITION_TYPED | content::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    d->webContents->GetController().LoadURLWithParams(params);
    d->webContents->Focus();
}

void WebContentsAdapter::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    Q_D(WebContentsAdapter);
    QByteArray encodedData = data.toPercentEncoding();
    std::string urlString("data:");
    urlString.append(mimeType.toStdString());
    urlString.append(",");
    urlString.append(encodedData.constData(), encodedData.length());

    content::NavigationController::LoadURLParams params((GURL(urlString)));
    params.load_type = content::NavigationController::LOAD_TYPE_DATA;
    params.base_url_for_data_url = toGurl(baseUrl);
    params.virtual_url_for_data_url = baseUrl.isEmpty() ? GURL(url::kAboutBlankURL) : toGurl(baseUrl);
    params.can_load_local_resources = true;
    d->webContents->GetController().LoadURLWithParams(params);
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
    return toQt(d->webContents->GetRenderViewHost()->GetView()->GetSelectedText());
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

void WebContentsAdapter::navigateToIndex(int offset)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().GoToIndex(offset);
    d->webContents->Focus();
}

void WebContentsAdapter::navigateToOffset(int offset)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetController().GoToOffset(offset);
    d->webContents->Focus();
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
    ::serializeNavigationHistory(d->webContents->GetController(), output);
}

void WebContentsAdapter::setZoomFactor(qreal factor)
{
    Q_D(WebContentsAdapter);
    if (factor < content::kMinimumZoomFactor || factor > content::kMaximumZoomFactor)
        return;
    content::HostZoomMap::SetZoomLevel(d->webContents.get(), content::ZoomFactorToZoomLevel(static_cast<double>(factor)));
}

qreal WebContentsAdapter::currentZoomFactor() const
{
    Q_D(const WebContentsAdapter);
    return content::ZoomLevelToZoomFactor(content::HostZoomMap::GetZoomLevel(d->webContents.get()));
}

void WebContentsAdapter::enableInspector(bool enable)
{
    ContentBrowserClientQt::Get()->enableInspector(enable);
}

QAccessibleInterface *WebContentsAdapter::browserAccessible()
{
    Q_D(const WebContentsAdapter);
    RenderWidgetHostViewQt *rwhv = static_cast<RenderWidgetHostViewQt*>(d->webContents->GetRenderWidgetHostView());
    return rwhv ? rwhv->GetQtAccessible() : Q_NULLPTR;
}

void WebContentsAdapter::runJavaScript(const QString &javaScript)
{
    Q_D(WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    rvh->GetMainFrame()->ExecuteJavaScript(toString16(javaScript));
}

quint64 WebContentsAdapter::runJavaScriptCallbackResult(const QString &javaScript)
{
    Q_D(WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    content::RenderFrameHost::JavaScriptResultCallback callback = base::Bind(&callbackOnEvaluateJS, d->adapterClient, d->nextRequestId);
    rvh->GetMainFrame()->ExecuteJavaScript(toString16(javaScript), callback);
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
    d->webContents->StopFinding(content::STOP_FIND_ACTION_KEEP_SELECTION);
}

void WebContentsAdapter::updateWebPreferences(const WebPreferences & webPreferences)
{
    Q_D(WebContentsAdapter);
    d->webContents->GetRenderViewHost()->UpdateWebkitPreferences(webPreferences);
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

void WebContentsAdapter::grantMediaAccessPermission(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags flags)
{
    Q_D(WebContentsAdapter);
    MediaCaptureDevicesDispatcher::GetInstance()->handleMediaAccessPermissionResponse(d->webContents.get(), securityOrigin, flags);
}

void WebContentsAdapter::dpiScaleChanged()
{
    Q_D(WebContentsAdapter);
    content::RenderWidgetHostImpl* impl = NULL;
    if (d->webContents->GetRenderViewHost())
        impl = content::RenderWidgetHostImpl::From(d->webContents->GetRenderViewHost());
    if (impl)
        impl->NotifyScreenInfoChanged();
}

void WebContentsAdapter::filesSelectedInChooser(const QStringList &fileList, WebContentsAdapterClient::FileChooserMode mode)
{
    Q_D(WebContentsAdapter);
    content::RenderViewHost *rvh = d->webContents->GetRenderViewHost();
    Q_ASSERT(rvh);
    QStringList files(fileList);
    if (mode == WebContentsAdapterClient::UploadFolder && !fileList.isEmpty()
            && QFileInfo(fileList.first()).isDir()) // Enumerate the directory
        files = listRecursively(QDir(fileList.first()));
    rvh->FilesSelectedInChooser(toVector<ui::SelectedFileInfo>(files), static_cast<content::FileChooserParams::Mode>(mode));
}
