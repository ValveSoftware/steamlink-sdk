// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/constants.h"

namespace extensions {

const char kExtensionScheme[] = "chrome-extension";
const char kExtensionResourceScheme[] = "chrome-extension-resource";

const base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");
const base::FilePath::CharType kLocaleFolder[] =
    FILE_PATH_LITERAL("_locales");
const base::FilePath::CharType kMessagesFilename[] =
    FILE_PATH_LITERAL("messages.json");
const base::FilePath::CharType kPlatformSpecificFolder[] =
    FILE_PATH_LITERAL("_platform_specific");
const base::FilePath::CharType kMetadataFolder[] =
    FILE_PATH_LITERAL("_metadata");
const base::FilePath::CharType kVerifiedContentsFilename[] =
    FILE_PATH_LITERAL("verified_contents.json");
const base::FilePath::CharType kComputedHashesFilename[] =
    FILE_PATH_LITERAL("computed_hashes.json");

const char kInstallDirectoryName[] = "Extensions";

const char kTempExtensionName[] = "CRX_INSTALL";

const char kDecodedImagesFilename[] = "DECODED_IMAGES";

const char kDecodedMessageCatalogsFilename[] = "DECODED_MESSAGE_CATALOGS";

const char kGeneratedBackgroundPageFilename[] =
    "_generated_background_page.html";

const char kModulesDir[] = "_modules";

const base::FilePath::CharType kExtensionFileExtension[] =
    FILE_PATH_LITERAL(".crx");
const base::FilePath::CharType kExtensionKeyFileExtension[] =
    FILE_PATH_LITERAL(".pem");

// If auto-updates are turned on, default to running every 5 hours.
const int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

const char kLocalAppSettingsDirectoryName[] = "Local App Settings";
const char kLocalExtensionSettingsDirectoryName[] = "Local Extension Settings";
const char kSyncAppSettingsDirectoryName[] = "Sync App Settings";
const char kSyncExtensionSettingsDirectoryName[] = "Sync Extension Settings";
const char kManagedSettingsDirectoryName[] = "Managed Extension Settings";
const char kStateStoreName[] = "Extension State";
const char kRulesStoreName[] = "Extension Rules";
const char kWebStoreAppId[] = "ahfgeienlihckogmohjhadlkjgocpleb";

const char kMimeTypeJpeg[] = "image/jpeg";
const char kMimeTypePng[] = "image/png";

}  // namespace extensions

namespace extension_misc {

const char kPdfExtensionId[] = "mhjfbmdgcfjbbpaeojofohoefgiehjai";
const char kQuickOfficeComponentExtensionId[] =
    "bpmcpldpdmajfigpchkicefoigmkfalc";
const char kQuickOfficeInternalExtensionId[] =
    "ehibbfinohgbchlgdbfpikodjaojhccn";
const char kQuickOfficeExtensionId[] = "gbkeegbaiigmenfmjfclcdgdpimamgkj";
const char kMimeHandlerPrivateTestExtensionId[] =
    "oickdpebdnfbgkcaoklfcdhjniefkcji";

const char kProdHangoutsExtensionId[] = "nckgahadagoaajjgafhacjanaoiihapd";
const char* const kHangoutsExtensionIds[6] = {
    kProdHangoutsExtensionId,
    "ljclpkphhpbpinifbeabbhlfddcpfdde",  // Debug.
    "ppleadejekpmccmnpjdimmlfljlkdfej",  // Alpha.
    "eggnbpckecmjlblplehfpjjdhhidfdoj",  // Beta.
    "jfjjdfefebklmdbmenmlehlopoocnoeh",  // Packaged App Debug.
    "knipolnnllmklapflnccelgolnpehhpl"   // Packaged App Prod.
    // Keep in sync with _api_features.json and _manifest_features.json.
};

}  // namespace extension_misc
