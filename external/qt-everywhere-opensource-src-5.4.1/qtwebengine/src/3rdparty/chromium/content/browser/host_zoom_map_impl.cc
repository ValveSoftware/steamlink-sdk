// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include <algorithm>
#include <cmath>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/page_zoom.h"
#include "net/base/net_util.h"

static const char* kHostZoomMapKeyName = "content_host_zoom_map";

namespace content {

namespace {

std::string GetHostFromProcessView(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return std::string();

  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);

  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry)
    return std::string();

  return net::GetHostOrSpecFromURL(entry->GetURL());
}

}  // namespace

HostZoomMap* HostZoomMap::GetForBrowserContext(BrowserContext* context) {
  HostZoomMapImpl* rv = static_cast<HostZoomMapImpl*>(
      context->GetUserData(kHostZoomMapKeyName));
  if (!rv) {
    rv = new HostZoomMapImpl();
    context->SetUserData(kHostZoomMapKeyName, rv);
  }
  return rv;
}

// Helper function for setting/getting zoom levels for WebContents without
// having to import HostZoomMapImpl everywhere.
double HostZoomMap::GetZoomLevel(const WebContents* web_contents) {
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(web_contents->GetBrowserContext()));
  return host_zoom_map->GetZoomLevelForWebContents(
      *static_cast<const WebContentsImpl*>(web_contents));
}

void HostZoomMap::SetZoomLevel(const WebContents* web_contents, double level) {
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(web_contents->GetBrowserContext()));
  host_zoom_map->SetZoomLevelForWebContents(
      *static_cast<const WebContentsImpl*>(web_contents), level);
}

HostZoomMapImpl::HostZoomMapImpl()
    : default_zoom_level_(0.0) {
  registrar_.Add(
      this, NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
      NotificationService::AllSources());
}

void HostZoomMapImpl::CopyFrom(HostZoomMap* copy_interface) {
  // This can only be called on the UI thread to avoid deadlocks, otherwise
  //   UI: a.CopyFrom(b);
  //   IO: b.CopyFrom(a);
  // can deadlock.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HostZoomMapImpl* copy = static_cast<HostZoomMapImpl*>(copy_interface);
  base::AutoLock auto_lock(lock_);
  base::AutoLock copy_auto_lock(copy->lock_);
  host_zoom_levels_.
      insert(copy->host_zoom_levels_.begin(), copy->host_zoom_levels_.end());
  for (SchemeHostZoomLevels::const_iterator i(copy->
           scheme_host_zoom_levels_.begin());
       i != copy->scheme_host_zoom_levels_.end(); ++i) {
    scheme_host_zoom_levels_[i->first] = HostZoomLevels();
    scheme_host_zoom_levels_[i->first].
        insert(i->second.begin(), i->second.end());
  }
  default_zoom_level_ = copy->default_zoom_level_;
}

double HostZoomMapImpl::GetZoomLevelForHost(const std::string& host) const {
  base::AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? default_zoom_level_ : i->second;
}

bool HostZoomMapImpl::HasZoomLevel(const std::string& scheme,
                                   const std::string& host) const {
  base::AutoLock auto_lock(lock_);

  SchemeHostZoomLevels::const_iterator scheme_iterator(
      scheme_host_zoom_levels_.find(scheme));

  const HostZoomLevels& zoom_levels =
      (scheme_iterator != scheme_host_zoom_levels_.end())
          ? scheme_iterator->second
          : host_zoom_levels_;

  HostZoomLevels::const_iterator i(zoom_levels.find(host));
  return i != zoom_levels.end();
}

double HostZoomMapImpl::GetZoomLevelForHostAndScheme(
    const std::string& scheme,
    const std::string& host) const {
  {
    base::AutoLock auto_lock(lock_);
    SchemeHostZoomLevels::const_iterator scheme_iterator(
        scheme_host_zoom_levels_.find(scheme));
    if (scheme_iterator != scheme_host_zoom_levels_.end()) {
      HostZoomLevels::const_iterator i(scheme_iterator->second.find(host));
      if (i != scheme_iterator->second.end())
        return i->second;
    }
  }
  return GetZoomLevelForHost(host);
}

HostZoomMap::ZoomLevelVector HostZoomMapImpl::GetAllZoomLevels() const {
  HostZoomMap::ZoomLevelVector result;
  {
    base::AutoLock auto_lock(lock_);
    result.reserve(host_zoom_levels_.size() + scheme_host_zoom_levels_.size());
    for (HostZoomLevels::const_iterator i = host_zoom_levels_.begin();
         i != host_zoom_levels_.end();
         ++i) {
      ZoomLevelChange change = {HostZoomMap::ZOOM_CHANGED_FOR_HOST,
                                i->first,       // host
                                std::string(),  // scheme
                                i->second       // zoom level
      };
      result.push_back(change);
    }
    for (SchemeHostZoomLevels::const_iterator i =
             scheme_host_zoom_levels_.begin();
         i != scheme_host_zoom_levels_.end();
         ++i) {
      const std::string& scheme = i->first;
      const HostZoomLevels& host_zoom_levels = i->second;
      for (HostZoomLevels::const_iterator j = host_zoom_levels.begin();
           j != host_zoom_levels.end();
           ++j) {
        ZoomLevelChange change = {HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST,
                                  j->first,  // host
                                  scheme,    // scheme
                                  j->second  // zoom level
        };
        result.push_back(change);
      }
    }
  }
  return result;
}

void HostZoomMapImpl::SetZoomLevelForHost(const std::string& host,
                                          double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);

    if (ZoomValuesEqual(level, default_zoom_level_))
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  // TODO(wjmaclean) Should we use a GURL here? crbug.com/384486
  SendZoomLevelChange(std::string(), host, level);

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_HOST;
  change.host = host;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::SetZoomLevelForHostAndScheme(const std::string& scheme,
                                                   const std::string& host,
                                                   double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    base::AutoLock auto_lock(lock_);
    scheme_host_zoom_levels_[scheme][host] = level;
  }

  SendZoomLevelChange(scheme, host, level);

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST;
  change.host = host;
  change.scheme = scheme;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

double HostZoomMapImpl::GetDefaultZoomLevel() const {
  return default_zoom_level_;
}

void HostZoomMapImpl::SetDefaultZoomLevel(double level) {
  default_zoom_level_ = level;
}

scoped_ptr<HostZoomMap::Subscription>
HostZoomMapImpl::AddZoomLevelChangedCallback(
    const ZoomLevelChangedCallback& callback) {
  return zoom_level_changed_callbacks_.Add(callback);
}

double HostZoomMapImpl::GetZoomLevelForWebContents(
    const WebContentsImpl& web_contents_impl) const {
  int render_process_id = web_contents_impl.GetRenderProcessHost()->GetID();
  int routing_id = web_contents_impl.GetRenderViewHost()->GetRoutingID();

  if (UsesTemporaryZoomLevel(render_process_id, routing_id))
    return GetTemporaryZoomLevel(render_process_id, routing_id);

  // Get the url from the navigation controller directly, as calling
  // WebContentsImpl::GetLastCommittedURL() may give us a virtual url that
  // is different than is stored in the map.
  GURL url;
  NavigationEntry* entry =
      web_contents_impl.GetController().GetLastCommittedEntry();
  // It is possible for a WebContent's zoom level to be queried before
  // a navigation has occurred.
  if (entry)
    url = entry->GetURL();
  return GetZoomLevelForHostAndScheme(url.scheme(),
                                      net::GetHostOrSpecFromURL(url));
}

void HostZoomMapImpl::SetZoomLevelForWebContents(
    const WebContentsImpl& web_contents_impl,
    double level) {
  int render_process_id = web_contents_impl.GetRenderProcessHost()->GetID();
  int render_view_id = web_contents_impl.GetRenderViewHost()->GetRoutingID();
  if (UsesTemporaryZoomLevel(render_process_id, render_view_id)) {
    SetTemporaryZoomLevel(render_process_id, render_view_id, level);
  } else {
    // Get the url from the navigation controller directly, as calling
    // WebContentsImpl::GetLastCommittedURL() may give us a virtual url that
    // is different than what the render view is using. If the two don't match,
    // the attempt to set the zoom will fail.
    NavigationEntry* entry =
        web_contents_impl.GetController().GetLastCommittedEntry();
    // Tests may invoke this function with a null entry, but we don't
    // want to save zoom levels in this case.
    if (!entry)
      return;

    GURL url = entry->GetURL();
    SetZoomLevelForHost(net::GetHostOrSpecFromURL(url), level);
  }
}

void HostZoomMapImpl::SetZoomLevelForView(int render_process_id,
                                          int render_view_id,
                                          double level,
                                          const std::string& host) {
  if (UsesTemporaryZoomLevel(render_process_id, render_view_id))
    SetTemporaryZoomLevel(render_process_id, render_view_id, level);
  else
    SetZoomLevelForHost(host, level);
}

bool HostZoomMapImpl::UsesTemporaryZoomLevel(int render_process_id,
                                             int render_view_id) const {
  RenderViewKey key(render_process_id, render_view_id);

  base::AutoLock auto_lock(lock_);
  return ContainsKey(temporary_zoom_levels_, key);
}

double HostZoomMapImpl::GetTemporaryZoomLevel(int render_process_id,
                                              int render_view_id) const {
  base::AutoLock auto_lock(lock_);
  RenderViewKey key(render_process_id, render_view_id);
  if (!ContainsKey(temporary_zoom_levels_, key))
    return 0;

  return temporary_zoom_levels_.find(key)->second;
}

void HostZoomMapImpl::SetTemporaryZoomLevel(int render_process_id,
                                            int render_view_id,
                                            double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    RenderViewKey key(render_process_id, render_view_id);
    base::AutoLock auto_lock(lock_);
    temporary_zoom_levels_[key] = level;
  }

  RenderViewHost* host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  host->Send(new ViewMsg_SetZoomLevelForView(render_view_id, true, level));

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM;
  change.host = GetHostFromProcessView(render_process_id, render_view_id);
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW: {
      int render_view_id = Source<RenderViewHost>(source)->GetRoutingID();
      int render_process_id =
          Source<RenderViewHost>(source)->GetProcess()->GetID();
      ClearTemporaryZoomLevel(render_process_id, render_view_id);
      break;
    }
    default:
      NOTREACHED() << "Unexpected preference observed.";
  }
}

void HostZoomMapImpl::ClearTemporaryZoomLevel(int render_process_id,
                                              int render_view_id) {
  {
    base::AutoLock auto_lock(lock_);
    RenderViewKey key(render_process_id, render_view_id);
    TemporaryZoomLevels::iterator it = temporary_zoom_levels_.find(key);
    if (it == temporary_zoom_levels_.end())
      return;
    temporary_zoom_levels_.erase(it);
  }
  RenderViewHost* host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  DCHECK(host);
  // Send a new zoom level, host-specific if one exists.
  host->Send(new ViewMsg_SetZoomLevelForView(
      render_view_id,
      false,
      GetZoomLevelForHost(
          GetHostFromProcessView(render_process_id, render_view_id))));
}

void HostZoomMapImpl::SendZoomLevelChange(const std::string& scheme,
                                          const std::string& host,
                                          double level) {
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    if (HostZoomMap::GetForBrowserContext(
            render_process_host->GetBrowserContext()) == this) {
      render_process_host->Send(
          new ViewMsg_SetZoomLevelForCurrentURL(scheme, host, level));
    }
  }
}

HostZoomMapImpl::~HostZoomMapImpl() {
}

}  // namespace content
