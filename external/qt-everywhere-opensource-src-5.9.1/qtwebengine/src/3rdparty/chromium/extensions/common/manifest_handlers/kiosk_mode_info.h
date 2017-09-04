// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_KIOSK_MODE_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_KIOSK_MODE_INFO_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct KioskModeInfo : public Extension::ManifestData {
 public:
  enum KioskStatus {
    NONE,
    ENABLED,
    ONLY
  };

  KioskModeInfo(KioskStatus kiosk_status,
                const std::vector<std::string>& secondary_app_ids,
                const std::string& required_platform_version);
  ~KioskModeInfo() override;

  // Gets the KioskModeInfo for |extension|, or NULL if none was
  // specified.
  static KioskModeInfo* Get(const Extension* extension);

  // Whether the extension or app is enabled for app kiosk mode.
  static bool IsKioskEnabled(const Extension* extension);

  // Whether the extension or app should only be available in kiosk mode.
  static bool IsKioskOnly(const Extension* extension);

  // Returns true if |extension| declares kiosk secondary apps.
  static bool HasSecondaryApps(const Extension* extension);

  // Whether the given |version_string| is a valid ChromeOS platform version.
  // The acceptable format is major[.minor[.micro]].
  static bool IsValidPlatformVersion(const std::string& version_string);

  KioskStatus kiosk_status;

  // The IDs of the kiosk secondary apps.
  const std::vector<std::string> secondary_app_ids;

  const std::string required_platform_version;
};

// Parses the "kiosk_enabled" and "kiosk_only" manifest keys.
class KioskModeHandler : public ManifestHandler {
 public:
  KioskModeHandler();
  ~KioskModeHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  const std::vector<std::string> Keys() const override;

  std::vector<std::string> supported_keys_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_KIOSK_MODE_INFO_H_
