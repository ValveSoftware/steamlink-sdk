// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/cloud_device_description.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_device_description_consts.h"

namespace cloud_devices {

CloudDeviceDescription::CloudDeviceDescription() {
  Reset();
}

CloudDeviceDescription::~CloudDeviceDescription() {
}

void CloudDeviceDescription::Reset() {
  root_.reset(new base::DictionaryValue);
  root_->SetString(json::kVersion, json::kVersion10);
}

bool CloudDeviceDescription::InitFromDictionary(
    std::unique_ptr<base::DictionaryValue> root) {
  if (!root)
    return false;
  Reset();
  root_ = std::move(root);
  std::string version;
  root_->GetString(json::kVersion, &version);
  return version == json::kVersion10;
}

bool CloudDeviceDescription::InitFromString(const std::string& json) {
  std::unique_ptr<base::Value> parsed = base::JSONReader::Read(json);
  base::DictionaryValue* description = NULL;
  if (!parsed || !parsed->GetAsDictionary(&description))
    return false;
  ignore_result(parsed.release());
  return InitFromDictionary(base::WrapUnique(description));
}

std::string CloudDeviceDescription::ToString() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *root_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

const base::DictionaryValue* CloudDeviceDescription::GetItem(
    const std::string& path) const {
  const base::DictionaryValue* value = NULL;
  root_->GetDictionary(path, &value);
  return value;
}

base::DictionaryValue* CloudDeviceDescription::CreateItem(
    const std::string& path) {
  base::DictionaryValue* value = new base::DictionaryValue;
  root_->Set(path, value);
  return value;
}

const base::ListValue* CloudDeviceDescription::GetListItem(
    const std::string& path) const {
  const base::ListValue* value = NULL;
  root_->GetList(path, &value);
  return value;
}

base::ListValue* CloudDeviceDescription::CreateListItem(
    const std::string& path) {
  base::ListValue* value = new base::ListValue;
  root_->Set(path, value);
  return value;
}

}  // namespace cloud_devices
