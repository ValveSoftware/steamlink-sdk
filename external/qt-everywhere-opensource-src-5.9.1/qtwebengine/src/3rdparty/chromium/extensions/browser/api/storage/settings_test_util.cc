// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/settings_test_util.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace settings_test_util {

// Creates a kilobyte of data.
std::unique_ptr<base::Value> CreateKilobyte() {
  std::string kilobyte_string;
  for (int i = 0; i < 1024; ++i) {
    kilobyte_string += "a";
  }
  return std::unique_ptr<base::Value>(new base::StringValue(kilobyte_string));
}

// Creates a megabyte of data.
std::unique_ptr<base::Value> CreateMegabyte() {
  base::ListValue* megabyte = new base::ListValue();
  for (int i = 0; i < 1000; ++i) {
    megabyte->Append(CreateKilobyte());
  }
  return std::unique_ptr<base::Value>(megabyte);
}

// Intended as a StorageCallback from GetStorage.
static void AssignStorage(ValueStore** dst, ValueStore* src) {
  *dst = src;
}

ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                       settings_namespace::Namespace settings_namespace,
                       StorageFrontend* frontend) {
  ValueStore* storage = NULL;
  frontend->RunWithStorage(
      extension, settings_namespace, base::Bind(&AssignStorage, &storage));
  base::RunLoop().RunUntilIdle();
  return storage;
}

ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                       StorageFrontend* frontend) {
  return GetStorage(extension, settings_namespace::SYNC, frontend);
}

scoped_refptr<const Extension> AddExtensionWithId(
    content::BrowserContext* context,
    const std::string& id,
    Manifest::Type type) {
  return AddExtensionWithIdAndPermissions(
      context, id, type, std::set<std::string>());
}

scoped_refptr<const Extension> AddExtensionWithIdAndPermissions(
    content::BrowserContext* context,
    const std::string& id,
    Manifest::Type type,
    const std::set<std::string>& permissions_set) {
  base::DictionaryValue manifest;
  manifest.SetString("name", std::string("Test extension ") + id);
  manifest.SetString("version", "1.0");

  std::unique_ptr<base::ListValue> permissions(new base::ListValue());
  for (std::set<std::string>::const_iterator it = permissions_set.begin();
      it != permissions_set.end(); ++it) {
    permissions->AppendString(*it);
  }
  manifest.Set("permissions", permissions.release());

  switch (type) {
    case Manifest::TYPE_EXTENSION:
      break;

    case Manifest::TYPE_LEGACY_PACKAGED_APP: {
      base::DictionaryValue* app = new base::DictionaryValue();
      base::DictionaryValue* app_launch = new base::DictionaryValue();
      app_launch->SetString("local_path", "fake.html");
      app->Set("launch", app_launch);
      manifest.Set("app", app);
      break;
    }

    default:
      NOTREACHED();
  }

  std::string error;
  scoped_refptr<const Extension> extension(
      Extension::Create(base::FilePath(),
                        Manifest::INTERNAL,
                        manifest,
                        Extension::NO_FLAGS,
                        id,
                        &error));
  DCHECK(extension.get());
  DCHECK(error.empty());

  // Ensure lookups via ExtensionRegistry (and ExtensionService) work even if
  // the test discards the referenced to the returned extension.
  ExtensionRegistry::Get(context)->AddEnabled(extension);

  for (std::set<std::string>::const_iterator it = permissions_set.begin();
      it != permissions_set.end(); ++it) {
    DCHECK(extension->permissions_data()->HasAPIPermission(*it));
  }

  return extension;
}

}  // namespace settings_test_util

}  // namespace extensions
