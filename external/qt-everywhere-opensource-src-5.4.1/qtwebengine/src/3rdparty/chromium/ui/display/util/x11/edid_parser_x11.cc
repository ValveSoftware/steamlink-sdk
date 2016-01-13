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

namespace ui {

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
  Atom* properties = XRRListOutputProperties(display, output, &num_properties);
  for (int i = 0; i < num_properties; ++i) {
    if (properties[i] == edid_property) {
      has_edid_property = true;
      break;
    }
  }
  XFree(properties);
  if (!has_edid_property)
    return false;

  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;
  unsigned long nitems = 0;
  unsigned char* prop = NULL;
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

// Gets some useful data from the specified output device, such like
// manufacturer's ID, product code, and human readable name. Returns false if it
// fails to get those data and doesn't touch manufacturer ID/product code/name.
// NULL can be passed for unwanted output parameters.
bool GetOutputDeviceData(XID output,
                         uint16_t* manufacturer_id,
                         std::string* human_readable_name) {
  std::vector<uint8_t> edid;
  if (!GetEDIDProperty(output, &edid))
    return false;

  bool result = ParseOutputDeviceData(
      edid, manufacturer_id, human_readable_name);
  return result;
}

}  // namespace

bool GetDisplayId(XID output_id,
                  uint8_t output_index,
                  int64_t* display_id_out) {
  std::vector<uint8_t> edid;
  if (!GetEDIDProperty(output_id, &edid))
    return false;

  bool result = GetDisplayIdFromEDID(edid, output_index, display_id_out);
  return result;
}

std::string GetDisplayName(RROutput output) {
  std::string display_name;
  GetOutputDeviceData(output, NULL, &display_name);
  return display_name;
}

bool GetOutputOverscanFlag(RROutput output, bool* flag) {
  std::vector<uint8_t> edid;
  if (!GetEDIDProperty(output, &edid))
    return false;

  bool found = ParseOutputOverscanFlag(edid, flag);
  return found;
}

}  // namespace ui
