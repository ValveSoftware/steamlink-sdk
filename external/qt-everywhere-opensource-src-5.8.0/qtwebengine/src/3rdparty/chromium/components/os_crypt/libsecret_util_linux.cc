// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/libsecret_util_linux.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

//
// LibsecretLoader
//

decltype(
    &::secret_password_store_sync) LibsecretLoader::secret_password_store_sync;
decltype(
    &::secret_service_search_sync) LibsecretLoader::secret_service_search_sync;
decltype(
    &::secret_password_clear_sync) LibsecretLoader::secret_password_clear_sync;
decltype(&::secret_item_get_secret) LibsecretLoader::secret_item_get_secret;
decltype(&::secret_value_get_text) LibsecretLoader::secret_value_get_text;
decltype(
    &::secret_item_get_attributes) LibsecretLoader::secret_item_get_attributes;
decltype(&::secret_item_load_secret_sync)
    LibsecretLoader::secret_item_load_secret_sync;
decltype(&::secret_value_unref) LibsecretLoader::secret_value_unref;
decltype(
    &::secret_service_lookup_sync) LibsecretLoader::secret_service_lookup_sync;

bool LibsecretLoader::libsecret_loaded_ = false;

const LibsecretLoader::FunctionInfo LibsecretLoader::kFunctions[] = {
    {"secret_item_get_secret",
     reinterpret_cast<void**>(&secret_item_get_secret)},
    {"secret_item_get_attributes",
     reinterpret_cast<void**>(&secret_item_get_attributes)},
    {"secret_item_load_secret_sync",
     reinterpret_cast<void**>(&secret_item_load_secret_sync)},
    {"secret_password_clear_sync",
     reinterpret_cast<void**>(&secret_password_clear_sync)},
    {"secret_password_store_sync",
     reinterpret_cast<void**>(&secret_password_store_sync)},
    {"secret_service_lookup_sync",
     reinterpret_cast<void**>(&secret_service_lookup_sync)},
    {"secret_service_search_sync",
     reinterpret_cast<void**>(&secret_service_search_sync)},
    {"secret_value_get_text", reinterpret_cast<void**>(&secret_value_get_text)},
    {"secret_value_unref", reinterpret_cast<void**>(&secret_value_unref)},
};

// static
bool LibsecretLoader::EnsureLibsecretLoaded() {
  return LoadLibsecret() && LibsecretIsAvailable();
}

// static
bool LibsecretLoader::LoadLibsecret() {
  if (libsecret_loaded_)
    return true;

  static void* handle = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    // We wanted to use libsecret, but we couldn't load it. Warn, because
    // either the user asked for this, or we autodetected it incorrectly. (Or
    // the system has broken libraries, which is also good to warn about.)
    // TODO(crbug.com/607435): Channel this message to the user-facing log
    VLOG(1) << "Could not load libsecret-1.so.0: " << dlerror();
    return false;
  }

  for (const auto& function : kFunctions) {
    dlerror();
    *function.pointer = dlsym(handle, function.name);
    const char* error = dlerror();
    if (error) {
      VLOG(1) << "Unable to load symbol " << function.name << ": " << error;
      dlclose(handle);
      return false;
    }
  }

  libsecret_loaded_ = true;
  // We leak the library handle. That's OK: this function is called only once.
  return true;
}

// static
bool LibsecretLoader::LibsecretIsAvailable() {
  if (!libsecret_loaded_)
    return false;
  // A dummy query is made to check for availability, because libsecret doesn't
  // have a dedicated availability function. For performance reasons, the query
  // is meant to return an empty result.
  LibsecretAttributesBuilder attrs;
  attrs.Append("application", "chrome-string_to_get_empty_result");
  const SecretSchema kDummySchema = {
      "_chrome_dummy_schema",
      SECRET_SCHEMA_DONT_MATCH_NAME,
      {{"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
       {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}}};

  GError* error = nullptr;
  GList* found =
      secret_service_search_sync(nullptr,  // default secret service
                                 &kDummySchema, attrs.Get(), SECRET_SEARCH_ALL,
                                 nullptr,  // no cancellable ojbect
                                 &error);
  bool success = (error == nullptr);
  if (error)
    g_error_free(error);
  if (found)
    g_list_free(found);

  return success;
}

//
// LibsecretAttributesBuilder
//

LibsecretAttributesBuilder::LibsecretAttributesBuilder() {
  attrs_ = g_hash_table_new_full(g_str_hash, g_str_equal,
                                 nullptr,   // no deleter for keys
                                 nullptr);  // no deleter for values
}

LibsecretAttributesBuilder::~LibsecretAttributesBuilder() {
  g_hash_table_destroy(attrs_);
}

void LibsecretAttributesBuilder::Append(const std::string& name,
                                        const std::string& value) {
  name_values_.push_back(name);
  gpointer name_str =
      static_cast<gpointer>(const_cast<char*>(name_values_.back().c_str()));
  name_values_.push_back(value);
  gpointer value_str =
      static_cast<gpointer>(const_cast<char*>(name_values_.back().c_str()));
  g_hash_table_insert(attrs_, name_str, value_str);
}

void LibsecretAttributesBuilder::Append(const std::string& name,
                                        int64_t value) {
  Append(name, base::Int64ToString(value));
}
