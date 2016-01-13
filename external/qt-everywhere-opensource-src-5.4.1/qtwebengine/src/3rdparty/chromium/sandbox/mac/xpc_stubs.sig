// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains declarations of public XPC functions used in the sandbox.
// This file is used with tools/generate_stubs for creating a dynamic library
// loader.

// XPC object management.
void xpc_release(xpc_object_t object);

// Dictionary manipulation.
const char* xpc_dictionary_get_string(xpc_object_t dictionary, const char* key);
uint64_t xpc_dictionary_get_uint64(xpc_object_t dictionary, const char* key);
void xpc_dictionary_set_int64(xpc_object_t dictionary, const char* key, int64_t value);
xpc_object_t xpc_dictionary_create_reply(xpc_object_t request);
