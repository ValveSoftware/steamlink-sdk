// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_THUNK_GETTER_H_
#define CHROME_ELF_THUNK_GETTER_H_

namespace sandbox {
class ServiceResolverThunk;
}

// Creates a |ServiceResolverThunk| based on the OS version. Ownership of the
// resulting thunk is passed to the caller.
sandbox::ServiceResolverThunk* GetThunk(bool relaxed);

#endif  // CHROME_ELF_THUNK_GETTER_H_
