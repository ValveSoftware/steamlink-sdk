// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_report_descriptor.h"

#include "base/stl_util.h"

namespace device {

HidReportDescriptor::HidReportDescriptor(const uint8_t* bytes, size_t size) {
  size_t header_index = 0;
  HidReportDescriptorItem* item = NULL;
  while (header_index < size) {
    item = new HidReportDescriptorItem(&bytes[header_index], item);
    items_.push_back(linked_ptr<HidReportDescriptorItem>(item));
    header_index += item->GetSize();
  }
}

HidReportDescriptor::~HidReportDescriptor() {}

void HidReportDescriptor::GetTopLevelCollections(
    std::vector<HidUsageAndPage>* topLevelCollections) {
  DCHECK(topLevelCollections);
  STLClearObject(topLevelCollections);

  for (std::vector<linked_ptr<HidReportDescriptorItem> >::const_iterator
           items_iter = items().begin();
       items_iter != items().end();
       ++items_iter) {
    linked_ptr<HidReportDescriptorItem> item = *items_iter;

    bool isTopLevelCollection =
        item->tag() == HidReportDescriptorItem::kTagCollection &&
        item->parent() == NULL;

    if (isTopLevelCollection) {
      uint16_t collection_usage = 0;
      HidUsageAndPage::Page collection_usage_page =
          HidUsageAndPage::kPageUndefined;

      HidReportDescriptorItem* usage = item->previous();
      if (usage && usage->tag() == HidReportDescriptorItem::kTagUsage) {
        collection_usage = usage->GetShortData();
      }

      HidReportDescriptorItem* usage_page = usage->previous();
      if (usage_page &&
          usage_page->tag() == HidReportDescriptorItem::kTagUsagePage) {
        collection_usage_page =
            (HidUsageAndPage::Page)usage_page->GetShortData();
      }

      topLevelCollections->push_back(
          HidUsageAndPage(collection_usage, collection_usage_page));
    }
  }
}

}  // namespace device
