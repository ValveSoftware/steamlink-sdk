// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOJO_STASH_BACKEND_H_
#define EXTENSIONS_BROWSER_MOJO_STASH_BACKEND_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/mojo/stash.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace extensions {

// A backend that provides access to StashService for a single extension.
class StashBackend {
 public:
  explicit StashBackend(const base::Closure& on_handle_readable);
  ~StashBackend();

  // Creates a StashService that forwards calls to this StashBackend and bind it
  // to |request|.
  void BindToRequest(mojo::InterfaceRequest<StashService> request);

  // Adds the StashedObjects contained within |stash| to the stash.
  void AddToStash(mojo::Array<StashedObjectPtr> stash);

  // Returns all StashedObjects added to the stash since the last call to
  // RetrieveStash.
  mojo::Array<StashedObjectPtr> RetrieveStash();

 private:
  class StashEntry;

  // Invoked when a handle is readable.
  void OnHandleReady();

  // The objects that have been stashed.
  std::vector<std::unique_ptr<StashEntry>> stashed_objects_;

  // The callback to call when a handle is readable.
  const base::Closure on_handle_readable_;

  // Whether a handle has become readable since the last RetrieveStash() call.
  bool has_notified_;

  base::WeakPtrFactory<StashBackend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StashBackend);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOJO_STASH_BACKEND_H_
