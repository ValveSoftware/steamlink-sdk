// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_IRT_PPAPI_H_
#define PPAPI_NACL_IRT_IRT_PPAPI_H_

extern "C" int irt_ppapi_start(const struct PP_StartFunctions* funcs);

size_t chrome_irt_query(const char* interface_ident,
                        void* table, size_t tablesize);

#endif  // PPAPI_NACL_IRT_IRT_PPAPI_H_
