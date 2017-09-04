// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_report_descriptor.h"

#include "base/stl_util.h"

namespace device {

namespace {

const int kBitsPerByte = 8;

}  // namespace

HidReportDescriptor::HidReportDescriptor(const std::vector<uint8_t>& bytes) {
  size_t header_index = 0;
  HidReportDescriptorItem* item = NULL;
  while (header_index < bytes.size()) {
    item = new HidReportDescriptorItem(&bytes[header_index],
                                       bytes.size() - header_index, item);
    items_.push_back(linked_ptr<HidReportDescriptorItem>(item));
    header_index += item->GetSize();
  }
}

HidReportDescriptor::~HidReportDescriptor() {}

void HidReportDescriptor::GetDetails(
    std::vector<HidCollectionInfo>* top_level_collections,
    bool* has_report_id,
    size_t* max_input_report_size,
    size_t* max_output_report_size,
    size_t* max_feature_report_size) {
  DCHECK(top_level_collections);
  DCHECK(max_input_report_size);
  DCHECK(max_output_report_size);
  DCHECK(max_feature_report_size);
  base::STLClearObject(top_level_collections);

  *has_report_id = false;
  *max_input_report_size = 0;
  *max_output_report_size = 0;
  *max_feature_report_size = 0;

  // Global tags data:
  HidUsageAndPage::Page current_usage_page = HidUsageAndPage::kPageUndefined;
  size_t current_report_count = 0;
  size_t cached_report_count = 0;
  size_t current_report_size = 0;
  size_t cached_report_size = 0;
  size_t current_input_report_size = 0;
  size_t current_output_report_size = 0;
  size_t current_feature_report_size = 0;

  // Local tags data:
  uint32_t current_usage = 0;

  for (std::vector<linked_ptr<HidReportDescriptorItem> >::const_iterator
           items_iter = items().begin();
       items_iter != items().end();
       ++items_iter) {
    linked_ptr<HidReportDescriptorItem> current_item = *items_iter;

    switch (current_item->tag()) {
      // Main tags:
      case HidReportDescriptorItem::kTagCollection:
        if (!current_item->parent() &&
            (current_usage <= std::numeric_limits<uint16_t>::max())) {
          // This is a top-level collection.
          HidCollectionInfo collection;
          collection.usage = HidUsageAndPage(
              static_cast<uint16_t>(current_usage), current_usage_page);
          top_level_collections->push_back(collection);
        }
        break;
      case HidReportDescriptorItem::kTagInput:
        current_input_report_size += current_report_count * current_report_size;
        break;
      case HidReportDescriptorItem::kTagOutput:
        current_output_report_size +=
            current_report_count * current_report_size;
        break;
      case HidReportDescriptorItem::kTagFeature:
        current_feature_report_size +=
            current_report_count * current_report_size;
        break;

      // Global tags:
      case HidReportDescriptorItem::kTagUsagePage:
        current_usage_page =
            static_cast<HidUsageAndPage::Page>(current_item->GetShortData());
        break;
      case HidReportDescriptorItem::kTagReportId:
        if (top_level_collections->size() > 0) {
          // Store report ID.
          top_level_collections->back().report_ids.insert(
              current_item->GetShortData());
          *has_report_id = true;

          // Update max report sizes.
          *max_input_report_size =
              std::max(*max_input_report_size, current_input_report_size);
          *max_output_report_size =
              std::max(*max_output_report_size, current_output_report_size);
          *max_feature_report_size =
              std::max(*max_feature_report_size, current_feature_report_size);

          // Reset the report sizes for the next report ID.
          current_input_report_size = 0;
          current_output_report_size = 0;
          current_feature_report_size = 0;
        }
        break;
      case HidReportDescriptorItem::kTagReportCount:
        current_report_count = current_item->GetShortData();
        break;
      case HidReportDescriptorItem::kTagReportSize:
        current_report_size = current_item->GetShortData();
        break;
      case HidReportDescriptorItem::kTagPush:
        // Cache report count and size.
        cached_report_count = current_report_count;
        cached_report_size = current_report_size;
        break;
      case HidReportDescriptorItem::kTagPop:
        // Restore cache.
        current_report_count = cached_report_count;
        current_report_size = cached_report_size;
        // Reset cache.
        cached_report_count = 0;
        cached_report_size = 0;
        break;

      // Local tags:
      case HidReportDescriptorItem::kTagUsage:
        current_usage = current_item->GetShortData();
        break;

      default:
        break;
    }
  }

  // Update max report sizes
  *max_input_report_size =
      std::max(*max_input_report_size, current_input_report_size);
  *max_output_report_size =
      std::max(*max_output_report_size, current_output_report_size);
  *max_feature_report_size =
      std::max(*max_feature_report_size, current_feature_report_size);

  // Convert bits into bytes
  *max_input_report_size /= kBitsPerByte;
  *max_output_report_size /= kBitsPerByte;
  *max_feature_report_size /= kBitsPerByte;
}

}  // namespace device
