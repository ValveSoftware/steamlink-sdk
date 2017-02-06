// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sessions/sessions_api.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/sessions/session_id.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "ui/base/layout.h"

namespace extensions {

namespace GetRecentlyClosed = api::sessions::GetRecentlyClosed;
namespace GetDevices = api::sessions::GetDevices;
namespace Restore = api::sessions::Restore;
namespace tabs = api::tabs;
namespace windows = api::windows;

const char kNoRecentlyClosedSessionsError[] =
    "There are no recently closed sessions.";
const char kInvalidSessionIdError[] = "Invalid session id: \"*\".";
const char kNoBrowserToRestoreSession[] =
    "There are no browser windows to restore the session.";
const char kSessionSyncError[] = "Synced sessions are not available.";
const char kRestoreInIncognitoError[] =
    "Can not restore sessions in incognito mode.";

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const sync_driver::SyncedSession* s1,
                           const sync_driver::SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

// Comparator function for use with std::sort that will sort tabs in a window
// by descending timestamp (i.e., most recent first).
bool SortTabsByRecency(const sessions::SessionTab* t1,
                       const sessions::SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

tabs::Tab CreateTabModelHelper(
    Profile* profile,
    const sessions::SerializedNavigationEntry& current_navigation,
    const std::string& session_id,
    int index,
    bool pinned,
    int selected_index,
    const Extension* extension) {
  tabs::Tab tab_struct;

  const GURL& url = current_navigation.virtual_url();
  std::string title = base::UTF16ToUTF8(current_navigation.title());

  tab_struct.session_id.reset(new std::string(session_id));
  tab_struct.url.reset(new std::string(url.spec()));
  tab_struct.fav_icon_url.reset(
      new std::string(current_navigation.favicon_url().spec()));
  if (!title.empty()) {
    tab_struct.title.reset(new std::string(title));
  } else {
    tab_struct.title.reset(new std::string(
        base::UTF16ToUTF8(url_formatter::FormatUrl(url))));
  }
  tab_struct.index = index;
  tab_struct.pinned = pinned;
  // Note: |selected_index| from the sync sessions model is what we call
  // "active" in extensions terminology.  "selected" is deprecated because it's
  // not clear whether it means "active" (user can see) or "highlighted" (user
  // has highlighted, since you can select tabs without bringing them into the
  // foreground).
  tab_struct.active = index == selected_index;
  ExtensionTabUtil::ScrubTabForExtension(extension, nullptr, &tab_struct);
  return tab_struct;
}

std::unique_ptr<windows::Window> CreateWindowModelHelper(
    std::unique_ptr<std::vector<tabs::Tab>> tabs,
    const std::string& session_id,
    const windows::WindowType& type,
    const windows::WindowState& state) {
  std::unique_ptr<windows::Window> window_struct(new windows::Window);
  window_struct->tabs = std::move(tabs);
  window_struct->session_id.reset(new std::string(session_id));
  window_struct->incognito = false;
  window_struct->always_on_top = false;
  window_struct->focused = false;
  window_struct->type = type;
  window_struct->state = state;
  return window_struct;
}

std::unique_ptr<api::sessions::Session> CreateSessionModelHelper(
    int last_modified,
    std::unique_ptr<tabs::Tab> tab,
    std::unique_ptr<windows::Window> window) {
  std::unique_ptr<api::sessions::Session> session_struct(
      new api::sessions::Session());
  session_struct->last_modified = last_modified;
  if (tab)
    session_struct->tab = std::move(tab);
  else if (window)
    session_struct->window = std::move(window);
  else
    NOTREACHED();
  return session_struct;
}

bool is_tab_entry(const sessions::TabRestoreService::Entry* entry) {
  return entry->type == sessions::TabRestoreService::TAB;
}

bool is_window_entry(const sessions::TabRestoreService::Entry* entry) {
  return entry->type == sessions::TabRestoreService::WINDOW;
}

tabs::Tab SessionsGetRecentlyClosedFunction::CreateTabModel(
    const sessions::TabRestoreService::Tab& tab,
    int session_id,
    int selected_index) {
  return CreateTabModelHelper(GetProfile(),
                              tab.navigations[tab.current_navigation_index],
                              base::IntToString(session_id),
                              tab.tabstrip_index,
                              tab.pinned,
                              selected_index,
                              extension());
}

std::unique_ptr<windows::Window>
SessionsGetRecentlyClosedFunction::CreateWindowModel(
    const sessions::TabRestoreService::Window& window,
    int session_id) {
  DCHECK(!window.tabs.empty());

  std::unique_ptr<std::vector<tabs::Tab>> tabs(new std::vector<tabs::Tab>());
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    tabs->push_back(CreateTabModel(window.tabs[i], window.tabs[i].id,
                                   window.selected_tab_index));
  }

  return CreateWindowModelHelper(std::move(tabs), base::IntToString(session_id),
                                 windows::WINDOW_TYPE_NORMAL,
                                 windows::WINDOW_STATE_NORMAL);
}

std::unique_ptr<api::sessions::Session>
SessionsGetRecentlyClosedFunction::CreateSessionModel(
    const sessions::TabRestoreService::Entry* entry) {
  std::unique_ptr<tabs::Tab> tab;
  std::unique_ptr<windows::Window> window;
  switch (entry->type) {
    case sessions::TabRestoreService::TAB:
      tab.reset(new tabs::Tab(CreateTabModel(
          *static_cast<const sessions::TabRestoreService::Tab*>(entry),
          entry->id, -1)));
      break;
    case sessions::TabRestoreService::WINDOW:
      window = CreateWindowModel(
          *static_cast<const sessions::TabRestoreService::Window*>(entry),
          entry->id);
      break;
    default:
      NOTREACHED();
  }
  return CreateSessionModelHelper(entry->timestamp.ToTimeT(), std::move(tab),
                                  std::move(window));
}

bool SessionsGetRecentlyClosedFunction::RunSync() {
  std::unique_ptr<GetRecentlyClosed::Params> params(
      GetRecentlyClosed::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  if (params->filter && params->filter->max_results)
    max_results = *params->filter->max_results;
  EXTENSION_FUNCTION_VALIDATE(max_results >= 0 &&
      max_results <= api::sessions::MAX_SESSION_RESULTS);

  std::vector<api::sessions::Session> result;
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());

  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // incognito mode)
  if (!tab_restore_service) {
    DCHECK_NE(GetProfile(), GetProfile()->GetOriginalProfile())
        << "sessions::TabRestoreService expected for normal profiles";
    results_ = GetRecentlyClosed::Results::Create(result);
    return true;
  }

  // List of entries. They are ordered from most to least recent.
  // We prune the list to contain max 25 entries at any time and removes
  // uninteresting entries.
  for (const sessions::TabRestoreService::Entry* entry :
       tab_restore_service->entries()) {
    result.push_back(std::move(*CreateSessionModel(entry)));
  }

  results_ = GetRecentlyClosed::Results::Create(result);
  return true;
}

tabs::Tab SessionsGetDevicesFunction::CreateTabModel(
    const std::string& session_tag,
    const sessions::SessionTab& tab,
    int tab_index,
    int selected_index) {
  std::string session_id = SessionId(session_tag, tab.tab_id.id()).ToString();
  return CreateTabModelHelper(
      GetProfile(),
      tab.navigations[tab.normalized_navigation_index()],
      session_id,
      tab_index,
      tab.pinned,
      selected_index,
      extension());
}

std::unique_ptr<windows::Window> SessionsGetDevicesFunction::CreateWindowModel(
    const sessions::SessionWindow& window,
    const std::string& session_tag) {
  DCHECK(!window.tabs.empty());

  // Prune tabs that are not syncable or are NewTabPage. Then, sort the tabs
  // from most recent to least recent.
  std::vector<const sessions::SessionTab*> tabs_in_window;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    const sessions::SessionTab* tab = window.tabs[i];
    if (tab->navigations.empty())
      continue;
    const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->normalized_navigation_index());
    if (search::IsNTPURL(current_navigation.virtual_url(), GetProfile())) {
      continue;
    }
    tabs_in_window.push_back(tab);
  }
  if (tabs_in_window.empty())
    return std::unique_ptr<windows::Window>();
  std::sort(tabs_in_window.begin(), tabs_in_window.end(), SortTabsByRecency);

  std::unique_ptr<std::vector<tabs::Tab>> tabs(new std::vector<tabs::Tab>());
  for (size_t i = 0; i < tabs_in_window.size(); ++i) {
    tabs->push_back(CreateTabModel(session_tag, *tabs_in_window[i], i,
                                   window.selected_tab_index));
  }

  std::string session_id =
      SessionId(session_tag, window.window_id.id()).ToString();

  windows::WindowType type = windows::WINDOW_TYPE_NONE;
  switch (window.type) {
    case sessions::SessionWindow::TYPE_TABBED:
      type = windows::WINDOW_TYPE_NORMAL;
      break;
    case sessions::SessionWindow::TYPE_POPUP:
      type = windows::WINDOW_TYPE_POPUP;
      break;
  }

  windows::WindowState state = windows::WINDOW_STATE_NONE;
  switch (window.show_state) {
    case ui::SHOW_STATE_NORMAL:
    case ui::SHOW_STATE_DOCKED:
      state = windows::WINDOW_STATE_NORMAL;
      break;
    case ui::SHOW_STATE_MINIMIZED:
      state = windows::WINDOW_STATE_MINIMIZED;
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      state = windows::WINDOW_STATE_MAXIMIZED;
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      state = windows::WINDOW_STATE_FULLSCREEN;
      break;
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_END:
      break;
  }

  std::unique_ptr<windows::Window> window_struct(
      CreateWindowModelHelper(std::move(tabs), session_id, type, state));
  // TODO(dwankri): Dig deeper to resolve bounds not being optional, so closed
  // windows in GetRecentlyClosed can have set values in Window helper.
  window_struct->left.reset(new int(window.bounds.x()));
  window_struct->top.reset(new int(window.bounds.y()));
  window_struct->width.reset(new int(window.bounds.width()));
  window_struct->height.reset(new int(window.bounds.height()));

  return window_struct;
}

std::unique_ptr<api::sessions::Session>
SessionsGetDevicesFunction::CreateSessionModel(
    const sessions::SessionWindow& window,
    const std::string& session_tag) {
  std::unique_ptr<windows::Window> window_model(
      CreateWindowModel(window, session_tag));
  // There is a chance that after pruning uninteresting tabs the window will be
  // empty.
  return !window_model ? std::unique_ptr<api::sessions::Session>()
                       : CreateSessionModelHelper(window.timestamp.ToTimeT(),
                                                  std::unique_ptr<tabs::Tab>(),
                                                  std::move(window_model));
}

api::sessions::Device SessionsGetDevicesFunction::CreateDeviceModel(
    const sync_driver::SyncedSession* session) {
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  // Already validated in RunAsync().
  std::unique_ptr<GetDevices::Params> params(
      GetDevices::Params::Create(*args_));
  if (params->filter && params->filter->max_results)
    max_results = *params->filter->max_results;

  api::sessions::Device device_struct;
  device_struct.info = session->session_name;
  device_struct.device_name = session->session_name;

  for (sync_driver::SyncedSession::SyncedWindowMap::const_iterator it =
           session->windows.begin();
       it != session->windows.end() &&
       static_cast<int>(device_struct.sessions.size()) < max_results;
       ++it) {
    std::unique_ptr<api::sessions::Session> session_model(
        CreateSessionModel(*it->second, session->session_tag));
    if (session_model)
      device_struct.sessions.push_back(std::move(*session_model));
  }
  return device_struct;
}

bool SessionsGetDevicesFunction::RunSync() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(GetProfile());
  if (!(service && service->GetPreferredDataTypes().Has(syncer::SESSIONS))) {
    // Sync not enabled.
    results_ =
        GetDevices::Results::Create(std::vector<api::sessions::Device>());
    return true;
  }

  sync_driver::OpenTabsUIDelegate* open_tabs = service->GetOpenTabsUIDelegate();
  std::vector<const sync_driver::SyncedSession*> sessions;
  if (!(open_tabs && open_tabs->GetAllForeignSessions(&sessions))) {
    results_ =
        GetDevices::Results::Create(std::vector<api::sessions::Device>());
    return true;
  }

  std::unique_ptr<GetDevices::Params> params(
      GetDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->filter && params->filter->max_results) {
    EXTENSION_FUNCTION_VALIDATE(*params->filter->max_results >= 0 &&
        *params->filter->max_results <= api::sessions::MAX_SESSION_RESULTS);
  }

  std::vector<api::sessions::Device> result;
  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);
  for (size_t i = 0; i < sessions.size(); ++i)
    result.push_back(CreateDeviceModel(sessions[i]));

  results_ = GetDevices::Results::Create(result);
  return true;
}

void SessionsRestoreFunction::SetInvalidIdError(const std::string& invalid_id) {
  SetError(ErrorUtils::FormatErrorMessage(kInvalidSessionIdError, invalid_id));
}


void SessionsRestoreFunction::SetResultRestoredTab(
    content::WebContents* contents) {
  std::unique_ptr<tabs::Tab> tab(
      ExtensionTabUtil::CreateTabObject(contents, extension()));
  std::unique_ptr<api::sessions::Session> restored_session(
      CreateSessionModelHelper(base::Time::Now().ToTimeT(), std::move(tab),
                               std::unique_ptr<windows::Window>()));
  results_ = Restore::Results::Create(*restored_session);
}

bool SessionsRestoreFunction::SetResultRestoredWindow(int window_id) {
  WindowController* controller = NULL;
  if (!windows_util::GetWindowFromWindowID(this, window_id, 0, &controller)) {
    // error_ is set by GetWindowFromWindowId function call.
    return false;
  }
  std::unique_ptr<base::DictionaryValue> window_value(
      controller->CreateWindowValueWithTabs(extension()));
  std::unique_ptr<windows::Window> window(
      windows::Window::FromValue(*window_value));
  results_ = Restore::Results::Create(*CreateSessionModelHelper(
      base::Time::Now().ToTimeT(), std::unique_ptr<tabs::Tab>(),
      std::move(window)));
  return true;
}

bool SessionsRestoreFunction::RestoreMostRecentlyClosed(Browser* browser) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  sessions::TabRestoreService::Entries entries = tab_restore_service->entries();

  if (entries.empty()) {
    SetError(kNoRecentlyClosedSessionsError);
    return false;
  }

  bool is_window = is_window_entry(entries.front());
  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service->RestoreMostRecentEntry(context);
  DCHECK(restored_tabs.size());

  sessions::ContentLiveTab* first_tab =
      static_cast<sessions::ContentLiveTab*>(restored_tabs[0]);
  if (is_window) {
    return SetResultRestoredWindow(
        ExtensionTabUtil::GetWindowIdOfTab(first_tab->web_contents()));
  }

  SetResultRestoredTab(first_tab->web_contents());
  return true;
}

bool SessionsRestoreFunction::RestoreLocalSession(const SessionId& session_id,
                                                  Browser* browser) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  sessions::TabRestoreService::Entries entries = tab_restore_service->entries();

  if (entries.empty()) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  // Check if the recently closed list contains an entry with the provided id.
  bool is_window = false;
  for (sessions::TabRestoreService::Entries::iterator it = entries.begin();
       it != entries.end(); ++it) {
    if ((*it)->id == session_id.id()) {
      // The only time a full window is being restored is if the entry ID
      // matches the provided ID and the entry type is Window.
      is_window = is_window_entry(*it);
      break;
    }
  }

  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service->RestoreEntryById(context, session_id.id(), UNKNOWN);
  // If the ID is invalid, restored_tabs will be empty.
  if (restored_tabs.empty()) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  sessions::ContentLiveTab* first_tab =
      static_cast<sessions::ContentLiveTab*>(restored_tabs[0]);

  // Retrieve the window through any of the tabs in restored_tabs.
  if (is_window) {
    return SetResultRestoredWindow(
        ExtensionTabUtil::GetWindowIdOfTab(first_tab->web_contents()));
  }

  SetResultRestoredTab(first_tab->web_contents());
  return true;
}

bool SessionsRestoreFunction::RestoreForeignSession(const SessionId& session_id,
                                                    Browser* browser) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(GetProfile());
  if (!(service && service->GetPreferredDataTypes().Has(syncer::SESSIONS))) {
    SetError(kSessionSyncError);
    return false;
  }
  sync_driver::OpenTabsUIDelegate* open_tabs = service->GetOpenTabsUIDelegate();
  if (!open_tabs) {
    SetError(kSessionSyncError);
    return false;
  }

  const sessions::SessionTab* tab = NULL;
  if (open_tabs->GetForeignTab(session_id.session_tag(),
                               session_id.id(),
                               &tab)) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    content::WebContents* contents = tab_strip->GetActiveWebContents();

    content::WebContents* tab_contents =
        SessionRestore::RestoreForeignSessionTab(contents, *tab,
                                                 NEW_FOREGROUND_TAB);
    SetResultRestoredTab(tab_contents);
    return true;
  }

  // Restoring a full window.
  std::vector<const sessions::SessionWindow*> windows;
  if (!open_tabs->GetForeignSession(session_id.session_tag(), &windows)) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  std::vector<const sessions::SessionWindow*>::const_iterator window =
      windows.begin();
  while (window != windows.end()
         && (*window)->window_id.id() != session_id.id()) {
    ++window;
  }
  if (window == windows.end()) {
    SetInvalidIdError(session_id.ToString());
    return false;
  }

  // Only restore one window at a time.
  std::vector<Browser*> browsers = SessionRestore::RestoreForeignSessionWindows(
      GetProfile(), window, window + 1);
  // Will always create one browser because we only restore one window per call.
  DCHECK_EQ(1u, browsers.size());
  return SetResultRestoredWindow(ExtensionTabUtil::GetWindowId(browsers[0]));
}

bool SessionsRestoreFunction::RunSync() {
  std::unique_ptr<Restore::Params> params(Restore::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = chrome::FindBrowserWithProfile(GetProfile());
  if (!browser) {
    SetError(kNoBrowserToRestoreSession);
    return false;
  }

  if (GetProfile() != GetProfile()->GetOriginalProfile()) {
    SetError(kRestoreInIncognitoError);
    return false;
  }

  if (!params->session_id)
    return RestoreMostRecentlyClosed(browser);

  std::unique_ptr<SessionId> session_id(SessionId::Parse(*params->session_id));
  if (!session_id) {
    SetInvalidIdError(*params->session_id);
    return false;
  }

  return session_id->IsForeign() ?
      RestoreForeignSession(*session_id, browser)
      : RestoreLocalSession(*session_id, browser);
}

SessionsEventRouter::SessionsEventRouter(Profile* profile)
    : profile_(profile),
      tab_restore_service_(TabRestoreServiceFactory::GetForProfile(profile)) {
  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // incognito mode)
  if (tab_restore_service_) {
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}

SessionsEventRouter::~SessionsEventRouter() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void SessionsEventRouter::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  EventRouter::Get(profile_)->BroadcastEvent(base::WrapUnique(
      new Event(events::SESSIONS_ON_CHANGED,
                api::sessions::OnChanged::kEventName, std::move(args))));
}

void SessionsEventRouter::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

SessionsAPI::SessionsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)->RegisterObserver(this,
      api::sessions::OnChanged::kEventName);
}

SessionsAPI::~SessionsAPI() {
}

void SessionsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<SessionsAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<SessionsAPI>*
SessionsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void SessionsAPI::OnListenerAdded(const EventListenerInfo& details) {
  sessions_event_router_.reset(
      new SessionsEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
