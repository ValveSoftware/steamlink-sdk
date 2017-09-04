// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/x11/edid_parser_x11.h"

#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "base/strings/string_util.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/x/x11_types.h"

namespace display {

namespace {

bool IsRandRAvailable() {
  int randr_version_major = 0;
  int randr_version_minor = 0;
  static bool is_randr_available = XRRQueryVersion(
      gfx::GetXDisplay(), &randr_version_major, &randr_version_minor);
  return is_randr_available;
}

// Get the EDID data from the |output| and stores to |edid|.
// Returns true if EDID property is successfully obtained. Otherwise returns
// false and does not touch |edid|.
bool GetEDIDProperty(XID output, std::vector<uint8_t>* edid) {
  if (!IsRandRAvailable())
    return false;

  Display* display = gfx::GetXDisplay();

  static Atom edid_property = XInternAtom(
      gfx::GetXDisplay(),
      RR_PROPERTY_RANDR_EDID, false);

  bool has_edid_property = false;
  int num_properties = 0;
  gfx::XScopedPtr<Atom[]> properties(
      XRRListOutputProperties(display, output, &num_properties));
  for (int i = 0; i < num_properties; ++i) {
    if (properties[i] == edid_property) {
      has_edid_property = true;
      break;
    }
  }
  if (!has_edid_property)
    return false;

  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;
  unsigned long nitems = 0;
  unsigned char* prop = nullptr;
  XRRGetOutputProperty(display,
                       output,
                       edid_property,
                       0,                // offset
                       128,              // length
                       false,            // _delete
                       false,            // pending
                       AnyPropertyType,  // req_type
                       &actual_type,
                       &actual_format,
                       &nitems,
                       &bytes_after,
                       &prop);
  DCHECK_EQ(XA_INTEGER, actual_type);
  DCHECK_EQ(8, actual_format);
  edid->assign(prop, prop + nitems);
  XFree(prop);
  return true;
}

}  // namespace

EDIDParserX11::EDIDParserX11(XID output_id)
    : output_id_(output_id) {
  GetEDIDProperty(output_id_, &edid_);
}

EDIDParserX11::~EDIDParserX11() {
}

bool EDIDParserX11::GetDisplayId(uint8_t index, int64_t* out_display_id) const {
  if (edid_.empty())
    return false;

  return GetDisplayIdFromEDID(edid_, index, out_display_id, nullptr);
}

std::string EDIDParserX11::GetDisplayName() const {
  std::string display_name;
  ParseOutputDeviceData(edid_, nullptr, nullptr, &display_name, nullptr,
                        nullptr);
  return display_name;
}

bool EDIDParserX11::GetOutputOverscanFlag(bool* out_flag) const {
  if (edid_.empty())
    return false;

  return ParseOutputOverscanFlag(edid_, out_flag);
}

}  // namespace display
