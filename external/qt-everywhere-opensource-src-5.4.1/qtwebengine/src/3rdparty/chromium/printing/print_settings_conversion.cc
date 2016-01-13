// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"

namespace printing {

namespace {

void GetCustomMarginsFromJobSettings(const base::DictionaryValue& settings,
                                     PageSizeMargins* page_size_margins) {
  const base::DictionaryValue* custom_margins;
  if (!settings.GetDictionary(kSettingMarginsCustom, &custom_margins) ||
      !custom_margins->GetDouble(kSettingMarginTop,
                                 &page_size_margins->margin_top) ||
      !custom_margins->GetDouble(kSettingMarginBottom,
                                 &page_size_margins->margin_bottom) ||
      !custom_margins->GetDouble(kSettingMarginLeft,
                                 &page_size_margins->margin_left) ||
      !custom_margins->GetDouble(kSettingMarginRight,
                                 &page_size_margins->margin_right)) {
    NOTREACHED();
  }
}

void SetMarginsToJobSettings(const std::string& json_path,
                             const PageMargins& margins,
                             base::DictionaryValue* job_settings) {
  base::DictionaryValue* dict = new base::DictionaryValue;
  job_settings->Set(json_path, dict);
  dict->SetInteger(kSettingMarginTop, margins.top);
  dict->SetInteger(kSettingMarginBottom, margins.bottom);
  dict->SetInteger(kSettingMarginLeft, margins.left);
  dict->SetInteger(kSettingMarginRight, margins.right);
}

void SetSizeToJobSettings(const std::string& json_path,
                          const gfx::Size& size,
                          base::DictionaryValue* job_settings) {
  base::DictionaryValue* dict = new base::DictionaryValue;
  job_settings->Set(json_path, dict);
  dict->SetInteger("width", size.width());
  dict->SetInteger("height", size.height());
}

void SetRectToJobSettings(const std::string& json_path,
                          const gfx::Rect& rect,
                          base::DictionaryValue* job_settings) {
  base::DictionaryValue* dict = new base::DictionaryValue;
  job_settings->Set(json_path, dict);
  dict->SetInteger("x", rect.x());
  dict->SetInteger("y", rect.y());
  dict->SetInteger("width", rect.width());
  dict->SetInteger("height", rect.height());
}

}  // namespace

bool PrintSettingsFromJobSettings(const base::DictionaryValue& job_settings,
                                  PrintSettings* settings) {
  bool display_header_footer = false;
  if (!job_settings.GetBoolean(kSettingHeaderFooterEnabled,
                               &display_header_footer)) {
    return false;
  }
  settings->set_display_header_footer(display_header_footer);

  if (settings->display_header_footer()) {
    base::string16 title;
    base::string16 url;
    if (!job_settings.GetString(kSettingHeaderFooterTitle, &title) ||
        !job_settings.GetString(kSettingHeaderFooterURL, &url)) {
      return false;
    }
    settings->set_title(title);
    settings->set_url(url);
  }

  bool backgrounds = false;
  bool selection_only = false;
  if (!job_settings.GetBoolean(kSettingShouldPrintBackgrounds, &backgrounds) ||
      !job_settings.GetBoolean(kSettingShouldPrintSelectionOnly,
                               &selection_only)) {
    return false;
  }
  settings->set_should_print_backgrounds(backgrounds);
  settings->set_selection_only(selection_only);

  PrintSettings::RequestedMedia requested_media;
  const base::DictionaryValue* media_size_value = NULL;
  if (job_settings.GetDictionary(kSettingMediaSize, &media_size_value)) {
    int width_microns = 0;
    int height_microns = 0;
    if (media_size_value->GetInteger(kSettingMediaSizeWidthMicrons,
                                     &width_microns) &&
        media_size_value->GetInteger(kSettingMediaSizeHeightMicrons,
                                     &height_microns)) {
      requested_media.size_microns = gfx::Size(width_microns, height_microns);
    }
    std::string vendor_id;
    if (media_size_value->GetString(kSettingMediaSizeVendorId, &vendor_id) &&
        !vendor_id.empty()) {
      requested_media.vendor_id = vendor_id;
    }
  }
  settings->set_requested_media(requested_media);

  int margin_type = DEFAULT_MARGINS;
  if (!job_settings.GetInteger(kSettingMarginsType, &margin_type) ||
      (margin_type != DEFAULT_MARGINS &&
       margin_type != NO_MARGINS &&
       margin_type != CUSTOM_MARGINS &&
       margin_type != PRINTABLE_AREA_MARGINS)) {
    margin_type = DEFAULT_MARGINS;
  }
  settings->set_margin_type(static_cast<MarginType>(margin_type));

  if (margin_type == CUSTOM_MARGINS) {
    PageSizeMargins page_size_margins;
    GetCustomMarginsFromJobSettings(job_settings, &page_size_margins);

    PageMargins margins_in_points;
    margins_in_points.Clear();
    margins_in_points.top = page_size_margins.margin_top;
    margins_in_points.bottom = page_size_margins.margin_bottom;
    margins_in_points.left = page_size_margins.margin_left;
    margins_in_points.right = page_size_margins.margin_right;

    settings->SetCustomMargins(margins_in_points);
  }

  PageRanges new_ranges;
  const base::ListValue* page_range_array = NULL;
  if (job_settings.GetList(kSettingPageRange, &page_range_array)) {
    for (size_t index = 0; index < page_range_array->GetSize(); ++index) {
      const base::DictionaryValue* dict;
      if (!page_range_array->GetDictionary(index, &dict))
        continue;

      PageRange range;
      if (!dict->GetInteger(kSettingPageRangeFrom, &range.from) ||
          !dict->GetInteger(kSettingPageRangeTo, &range.to)) {
        continue;
      }

      // Page numbers are 1-based in the dictionary.
      // Page numbers are 0-based for the printing context.
      range.from--;
      range.to--;
      new_ranges.push_back(range);
    }
  }
  settings->set_ranges(new_ranges);

  int color = 0;
  bool landscape = false;
  int duplex_mode = 0;
  base::string16 device_name;
  bool collate = false;
  int copies = 1;

  if (!job_settings.GetBoolean(kSettingCollate, &collate) ||
      !job_settings.GetInteger(kSettingCopies, &copies) ||
      !job_settings.GetInteger(kSettingColor, &color) ||
      !job_settings.GetInteger(kSettingDuplexMode, &duplex_mode) ||
      !job_settings.GetBoolean(kSettingLandscape, &landscape) ||
      !job_settings.GetString(kSettingDeviceName, &device_name)) {
    return false;
  }

  settings->set_collate(collate);
  settings->set_copies(copies);
  settings->SetOrientation(landscape);
  settings->set_device_name(device_name);
  settings->set_duplex_mode(static_cast<DuplexMode>(duplex_mode));
  settings->set_color(static_cast<ColorModel>(color));

  return true;
}

void PrintSettingsToJobSettingsDebug(const PrintSettings& settings,
                                     base::DictionaryValue* job_settings) {
  job_settings->SetBoolean(kSettingHeaderFooterEnabled,
                           settings.display_header_footer());
  job_settings->SetString(kSettingHeaderFooterTitle, settings.title());
  job_settings->SetString(kSettingHeaderFooterURL, settings.url());
  job_settings->SetBoolean(kSettingShouldPrintBackgrounds,
                           settings.should_print_backgrounds());
  job_settings->SetBoolean(kSettingShouldPrintSelectionOnly,
                           settings.selection_only());
  job_settings->SetInteger(kSettingMarginsType, settings.margin_type());
  if (!settings.ranges().empty()) {
    base::ListValue* page_range_array = new base::ListValue;
    job_settings->Set(kSettingPageRange, page_range_array);
    for (size_t i = 0; i < settings.ranges().size(); ++i) {
      base::DictionaryValue* dict = new base::DictionaryValue;
      page_range_array->Append(dict);
      dict->SetInteger(kSettingPageRangeFrom, settings.ranges()[i].from + 1);
      dict->SetInteger(kSettingPageRangeTo, settings.ranges()[i].to + 1);
    }
  }

  job_settings->SetBoolean(kSettingCollate, settings.collate());
  job_settings->SetInteger(kSettingCopies, settings.copies());
  job_settings->SetInteger(kSettingColor, settings.color());
  job_settings->SetInteger(kSettingDuplexMode, settings.duplex_mode());
  job_settings->SetBoolean(kSettingLandscape, settings.landscape());
  job_settings->SetString(kSettingDeviceName, settings.device_name());

  // Following values are not read form JSON by InitSettings, so do not have
  // common public constants. So just serialize in "debug" section.
  base::DictionaryValue* debug = new base::DictionaryValue;
  job_settings->Set("debug", debug);
  debug->SetDouble("minShrink", settings.min_shrink());
  debug->SetDouble("maxShrink", settings.max_shrink());
  debug->SetInteger("desiredDpi", settings.desired_dpi());
  debug->SetInteger("dpi", settings.dpi());
  debug->SetInteger("deviceUnitsPerInch", settings.device_units_per_inch());
  debug->SetBoolean("support_alpha_blend", settings.should_print_backgrounds());
  debug->SetString("media_vendor_od", settings.requested_media().vendor_id);
  SetSizeToJobSettings(
      "media_size", settings.requested_media().size_microns, debug);
  SetMarginsToJobSettings("requested_custom_margins_in_points",
                          settings.requested_custom_margins_in_points(),
                          debug);
  const PageSetup& page_setup = settings.page_setup_device_units();
  SetMarginsToJobSettings(
      "effective_margins", page_setup.effective_margins(), debug);
  SetSizeToJobSettings("physical_size", page_setup.physical_size(), debug);
  SetRectToJobSettings("overlay_area", page_setup.overlay_area(), debug);
  SetRectToJobSettings("content_area", page_setup.content_area(), debug);
  SetRectToJobSettings("printable_area", page_setup.printable_area(), debug);
}

}  // namespace printing
