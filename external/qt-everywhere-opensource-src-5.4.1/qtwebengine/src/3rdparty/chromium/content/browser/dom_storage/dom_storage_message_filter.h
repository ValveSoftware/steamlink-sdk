// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_MESSAGE_FILTER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace base {
class NullableString16;
}

namespace content {

class DOMStorageArea;
class DOMStorageContextImpl;
class DOMStorageContextWrapper;
class DOMStorageHost;

// This class handles the logistics of DOM Storage within the browser process.
// It mostly ferries information between IPCs and the dom_storage classes.
class DOMStorageMessageFilter
    : public BrowserMessageFilter,
      public DOMStorageContextImpl::EventObserver {
 public:
  explicit DOMStorageMessageFilter(int render_process_id,
                                   DOMStorageContextWrapper* context);

 private:
  virtual ~DOMStorageMessageFilter();

  void InitializeInSequence();
  void UninitializeInSequence();

  // BrowserMessageFilter implementation
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message Handlers.
  void OnOpenStorageArea(int connection_id, int64 namespace_id,
                         const GURL& origin);
  void OnCloseStorageArea(int connection_id);
  void OnLoadStorageArea(int connection_id, DOMStorageValuesMap* map,
                         bool* send_log_get_messages);
  void OnSetItem(int connection_id, const base::string16& key,
                 const base::string16& value, const GURL& page_url);
  void OnLogGetItem(int connection_id, const base::string16& key,
                    const base::NullableString16& value);
  void OnRemoveItem(int connection_id, const base::string16& key,
                    const GURL& page_url);
  void OnClear(int connection_id, const GURL& page_url);
  void OnFlushMessages();

  // DOMStorageContextImpl::EventObserver implementation which
  // sends events back to our renderer process.
  virtual void OnDOMStorageItemSet(
      const DOMStorageArea* area,
      const base::string16& key,
      const base::string16& new_value,
      const base::NullableString16& old_value,
      const GURL& page_url) OVERRIDE;
  virtual void OnDOMStorageItemRemoved(
      const DOMStorageArea* area,
      const base::string16& key,
      const base::string16& old_value,
      const GURL& page_url) OVERRIDE;
  virtual void OnDOMStorageAreaCleared(
      const DOMStorageArea* area,
      const GURL& page_url) OVERRIDE;
  virtual void OnDOMSessionStorageReset(int64 namespace_id) OVERRIDE;

  void SendDOMStorageEvent(
      const DOMStorageArea* area,
      const GURL& page_url,
      const base::NullableString16& key,
      const base::NullableString16& new_value,
      const base::NullableString16& old_value);

  int render_process_id_;
  scoped_refptr<DOMStorageContextImpl> context_;
  scoped_ptr<DOMStorageHost> host_;
  int connection_dispatching_message_for_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_MESSAGE_FILTER_H_
