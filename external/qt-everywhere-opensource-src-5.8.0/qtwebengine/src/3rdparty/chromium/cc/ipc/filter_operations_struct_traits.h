// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_FILTER_OPERATIONS_STRUCT_TRAITS_H_
#define CC_IPC_FILTER_OPERATIONS_STRUCT_TRAITS_H_

#include "cc/ipc/filter_operations.mojom.h"
#include "cc/output/filter_operations.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::FilterOperations, cc::FilterOperations> {
  static const std::vector<cc::FilterOperation>& operations(
      const cc::FilterOperations& operations) {
    return operations.operations();
  }

  static bool Read(cc::mojom::FilterOperationsDataView data,
                   cc::FilterOperations* out) {
    std::vector<cc::FilterOperation> operations;
    if (!data.ReadOperations(&operations))
      return false;
    *out = cc::FilterOperations(std::move(operations));
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_FILTER_OPERATIONS_STRUCT_TRAITS_H_
