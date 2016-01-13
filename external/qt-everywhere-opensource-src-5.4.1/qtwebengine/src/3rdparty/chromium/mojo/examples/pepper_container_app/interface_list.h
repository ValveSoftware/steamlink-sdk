// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_INTERFACE_LIST_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_INTERFACE_LIST_H_

#include <map>
#include <string>

#include "base/macros.h"

namespace mojo {
namespace examples {

// InterfaceList maintains the mapping from Pepper interface names to
// interface pointers.
class InterfaceList {
 public:
  InterfaceList();
  ~InterfaceList();

  static InterfaceList* GetInstance();

  const void* GetBrowserInterface(const std::string& name) const;

 private:
  typedef std::map<std::string, const void*> NameToInterfaceMap;
  NameToInterfaceMap browser_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceList);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_INTERFACE_LIST_H_
