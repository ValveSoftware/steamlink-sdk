// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/browser_online_state_observer.h"

#include "content/common/view_messages.h"
#include "content/browser/renderer_host/render_process_host_impl.h"

namespace content {

BrowserOnlineStateObserver::BrowserOnlineStateObserver() {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

BrowserOnlineStateObserver::~BrowserOnlineStateObserver() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void BrowserOnlineStateObserver::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(new ViewMsg_NetworkTypeChanged(type));
  }
}

}  // namespace content
