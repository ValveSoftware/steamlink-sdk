// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_TYPES_H_
#define SERVICES_CATALOG_TYPES_H_

#include <map>
#include <memory>
#include <string>


namespace catalog {

class Entry;

// A map of mojo names -> catalog |Entry|s.
using EntryCache = std::map<std::string, std::unique_ptr<Entry>>;

}  // namespace catalog

#endif  // SERVICES_CATALOG_TYPES_H_
