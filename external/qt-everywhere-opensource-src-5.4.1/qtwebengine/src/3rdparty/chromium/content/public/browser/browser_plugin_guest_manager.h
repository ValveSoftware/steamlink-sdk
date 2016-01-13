// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {

class SiteInstance;
class WebContents;

// A BrowserPluginGuestManager offloads guest management and routing
// operations outside of the content layer.
class CONTENT_EXPORT BrowserPluginGuestManager {
 public:
  virtual ~BrowserPluginGuestManager() {}

  // Requests the allocation of a new guest WebContents.
  virtual content::WebContents* CreateGuest(
      content::SiteInstance* embedder_site_instance,
      int instance_id,
      scoped_ptr<base::DictionaryValue> extra_params);

  // Return a new instance ID.
  // TODO(fsamuel): Remove this. Once the instance ID concept is moved
  // entirely out of content and into chrome, this API will be unnecessary.
  virtual int GetNextInstanceID();

  typedef base::Callback<void(WebContents*)> GuestByInstanceIDCallback;
  // Requests a guest WebContents associated with the provided
  // |guest_instance_id|. If a guest associated with the provided ID
  // does not exist, then the |callback| will be called with a NULL
  // WebContents. If the provided |embedder_render_process_id| does
  // not own the requested guest, then the embedder will be killed,
  // and the |callback| will not be called.
  virtual void MaybeGetGuestByInstanceIDOrKill(
      int guest_instance_id,
      int embedder_render_process_id,
      const GuestByInstanceIDCallback& callback) {}

  // Iterates over all WebContents belonging to a given |embedder_web_contents|,
  // calling |callback| for each. If one of the callbacks returns true, then
  // the iteration exits early.
  typedef base::Callback<bool(WebContents*)> GuestCallback;
  virtual bool ForEachGuest(WebContents* embedder_web_contents,
                            const GuestCallback& callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
