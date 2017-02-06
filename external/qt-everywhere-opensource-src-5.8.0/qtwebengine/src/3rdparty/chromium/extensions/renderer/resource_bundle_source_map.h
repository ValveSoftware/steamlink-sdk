// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
#define EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "extensions/renderer/module_system.h"
#include "v8/include/v8.h"

namespace ui {
class ResourceBundle;
}

namespace extensions {

class ResourceBundleSourceMap : public extensions::ModuleSystem::SourceMap {
 public:
  explicit ResourceBundleSourceMap(const ui::ResourceBundle* resource_bundle);
  ~ResourceBundleSourceMap() override;

  v8::Local<v8::Value> GetSource(v8::Isolate* isolate,
                                 const std::string& name) const override;
  bool Contains(const std::string& name) const override;

  void RegisterSource(const std::string& name, int resource_id);

 private:
  const ui::ResourceBundle* resource_bundle_;
  std::map<std::string, int> resource_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundleSourceMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
