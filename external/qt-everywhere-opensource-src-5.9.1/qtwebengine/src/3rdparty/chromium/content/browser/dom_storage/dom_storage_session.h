// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_SESSION_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_SESSION_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class DOMStorageContextImpl;

// This refcounted class determines the lifetime of a session
// storage namespace and provides an interface to Clone() an
// existing session storage namespace. It may be used on any thread.
// See class comments for DOMStorageContextImpl for a larger overview.
class CONTENT_EXPORT DOMStorageSession
    : public base::RefCountedThreadSafe<DOMStorageSession> {
 public:
  // Constructs a |DOMStorageSession| and allocates new IDs for it.
  explicit DOMStorageSession(DOMStorageContextImpl* context);

  // Constructs a |DOMStorageSession| and assigns |persistent_namespace_id|
  // to it. Allocates a new non-persistent ID.
  DOMStorageSession(DOMStorageContextImpl* context,
                    const std::string& persistent_namespace_id);

  int64_t namespace_id() const { return namespace_id_; }
  const std::string& persistent_namespace_id() const {
    return persistent_namespace_id_;
  }
  void SetShouldPersist(bool should_persist);
  bool should_persist() const;
  bool IsFromContext(DOMStorageContextImpl* context);
  DOMStorageSession* Clone();

  // Constructs a |DOMStorageSession| by cloning
  // |namespace_id_to_clone|. Allocates new IDs for it.
  static DOMStorageSession* CloneFrom(DOMStorageContextImpl* context,
                                      int64_t namepace_id_to_clone);

 private:
  friend class base::RefCountedThreadSafe<DOMStorageSession>;

  DOMStorageSession(DOMStorageContextImpl* context,
                    int64_t namespace_id,
                    const std::string& persistent_namespace_id);
  ~DOMStorageSession();

  scoped_refptr<DOMStorageContextImpl> context_;
  int64_t namespace_id_;
  std::string persistent_namespace_id_;
  bool should_persist_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageSession);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_SESSION_H_
