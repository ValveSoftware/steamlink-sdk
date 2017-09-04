// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

namespace {
bool g_native_cups_enabled = false;
}  // namespace

namespace printing {

PrinterBasicInfo::PrinterBasicInfo()
    : printer_status(0),
      is_default(false) {}

PrinterBasicInfo::PrinterBasicInfo(const PrinterBasicInfo& other) = default;

PrinterBasicInfo::~PrinterBasicInfo() {}

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults()
    : collate_capable(false),
      collate_default(false),
      copies_capable(false),
      duplex_capable(false),
      duplex_default(UNKNOWN_DUPLEX_MODE),
      color_changeable(false),
      color_default(false),
      color_model(UNKNOWN_COLOR_MODEL),
      bw_model(UNKNOWN_COLOR_MODEL)
{}

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults(
    const PrinterSemanticCapsAndDefaults& other) = default;

PrinterSemanticCapsAndDefaults::~PrinterSemanticCapsAndDefaults() {}

PrinterCapsAndDefaults::PrinterCapsAndDefaults() {}

PrinterCapsAndDefaults::PrinterCapsAndDefaults(
    const PrinterCapsAndDefaults& other) = default;

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() {}

PrintBackend::~PrintBackend() {}

// static
bool PrintBackend::GetNativeCupsEnabled() {
  return g_native_cups_enabled;
}

// static
void PrintBackend::SetNativeCupsEnabled(bool enabled) {
  g_native_cups_enabled = enabled;
}

}  // namespace printing
