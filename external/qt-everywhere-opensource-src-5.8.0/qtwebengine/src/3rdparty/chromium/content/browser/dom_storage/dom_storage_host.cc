// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_host.h"

#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "url/gurl.h"

namespace content {

DOMStorageHost::DOMStorageHost(DOMStorageContextImpl* context)
    : context_(context) {
}

DOMStorageHost::~DOMStorageHost() {
  AreaMap::const_iterator it = connections_.begin();
  for (; it != connections_.end(); ++it)
    it->second.namespace_->CloseStorageArea(it->second.area_.get());
  connections_.clear();  // Clear prior to releasing the context_
}

bool DOMStorageHost::OpenStorageArea(int connection_id, int namespace_id,
                                     const GURL& origin) {
  DCHECK(!GetOpenArea(connection_id));
  if (GetOpenArea(connection_id))
    return false;  // Indicates the renderer gave us very bad data.
  NamespaceAndArea references;
  references.namespace_ = context_->GetStorageNamespace(namespace_id);
  if (!references.namespace_.get())
    return false;
  references.area_ = references.namespace_->OpenStorageArea(origin);
  DCHECK(references.area_.get());
  connections_[connection_id] = references;
  return true;
}

void DOMStorageHost::CloseStorageArea(int connection_id) {
  AreaMap::iterator found = connections_.find(connection_id);
  if (found == connections_.end())
    return;
  found->second.namespace_->CloseStorageArea(found->second.area_.get());
  connections_.erase(found);
}

bool DOMStorageHost::ExtractAreaValues(
    int connection_id, DOMStorageValuesMap* map) {
  map->clear();
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return false;
  if (!area->IsLoadedInMemory()) {
    DOMStorageNamespace* ns = GetNamespace(connection_id);
    DCHECK(ns);
    context_->PurgeMemory(DOMStorageContextImpl::PURGE_IF_NEEDED);
  }
  area->ExtractValues(map);
  return true;
}

unsigned DOMStorageHost::GetAreaLength(int connection_id) {
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return 0;
  return area->Length();
}

base::NullableString16 DOMStorageHost::GetAreaKey(int connection_id,
                                                  unsigned index) {
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return base::NullableString16();
  return area->Key(index);
}

base::NullableString16 DOMStorageHost::GetAreaItem(int connection_id,
                                                   const base::string16& key) {
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return base::NullableString16();
  return area->GetItem(key);
}

bool DOMStorageHost::SetAreaItem(
    int connection_id, const base::string16& key,
    const base::string16& value, const GURL& page_url,
    base::NullableString16* old_value) {
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return false;
  if (!area->SetItem(key, value, old_value))
    return false;
  if (old_value->is_null() || old_value->string() != value)
    context_->NotifyItemSet(area, key, value, *old_value, page_url);
  return true;
}

bool DOMStorageHost::RemoveAreaItem(
    int connection_id, const base::string16& key, const GURL& page_url,
    base::string16* old_value) {
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return false;
  if (!area->RemoveItem(key, old_value))
    return false;
  context_->NotifyItemRemoved(area, key, *old_value, page_url);
  return true;
}

bool DOMStorageHost::ClearArea(int connection_id, const GURL& page_url) {
  DOMStorageArea* area = GetOpenArea(connection_id);
  if (!area)
    return false;
  if (!area->Clear())
    return false;
  context_->NotifyAreaCleared(area, page_url);
  return true;
}

bool DOMStorageHost::HasAreaOpen(
    int namespace_id, const GURL& origin) const {
  AreaMap::const_iterator it = connections_.begin();
  for (; it != connections_.end(); ++it) {
    if (namespace_id == it->second.namespace_->namespace_id() &&
        origin == it->second.area_->origin()) {
      return true;
    }
  }
  return false;
}

DOMStorageArea* DOMStorageHost::GetOpenArea(int connection_id) {
  AreaMap::iterator found = connections_.find(connection_id);
  if (found == connections_.end())
    return NULL;
  return found->second.area_.get();
}

DOMStorageNamespace* DOMStorageHost::GetNamespace(int connection_id) {
  AreaMap::iterator found = connections_.find(connection_id);
  if (found == connections_.end())
    return NULL;
  return found->second.namespace_.get();
}

// NamespaceAndArea

DOMStorageHost::NamespaceAndArea::NamespaceAndArea() {}
DOMStorageHost::NamespaceAndArea::NamespaceAndArea(
    const NamespaceAndArea& other) = default;
DOMStorageHost::NamespaceAndArea::~NamespaceAndArea() {}

}  // namespace content
