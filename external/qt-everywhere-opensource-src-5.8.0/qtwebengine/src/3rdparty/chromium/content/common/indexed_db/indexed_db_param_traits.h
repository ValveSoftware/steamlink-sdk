// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_PARAM_TRAITS_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
}

namespace IPC {

template <>
struct ParamTraits<content::IndexedDBKey> {
  typedef content::IndexedDBKey param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<content::IndexedDBKeyRange> {
  typedef content::IndexedDBKeyRange param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<content::IndexedDBKeyPath> {
  typedef content::IndexedDBKeyPath param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_PARAM_TRAITS_H_
