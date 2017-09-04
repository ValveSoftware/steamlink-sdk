// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PARAM_TRAITS_H_
#define IPC_IPC_PARAM_TRAITS_H_

// Our IPC system uses the following partially specialized header to define how
// a data type is read, written and logged in the IPC system.

namespace IPC {

template <class P> struct ParamTraits {
};

template <class P>
struct SimilarTypeTraits {
  typedef P Type;
};

}  // namespace IPC

#endif  // IPC_IPC_PARAM_TRAITS_H_
