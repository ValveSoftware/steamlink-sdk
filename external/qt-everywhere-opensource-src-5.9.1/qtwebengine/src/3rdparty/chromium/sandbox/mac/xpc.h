// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides forward declarations for private XPC symbols.

#ifndef SANDBOX_MAC_XPC_H_
#define SANDBOX_MAC_XPC_H_

#include <AvailabilityMacros.h>
#include <mach/mach.h>
#include <bsm/libbsm.h>
#include <xpc/xpc.h>

#include "sandbox/sandbox_export.h"

// Declare private types.
extern "C" {
typedef struct _xpc_pipe_s* xpc_pipe_t;
}  // extern "C"

#if defined(MAC_OS_X_VERSION_10_7) && \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7
// Redeclare methods that only exist on 10.7+ to suppress
// -Wpartial-availability warnings.
extern "C" {
void xpc_dictionary_set_int64(xpc_object_t xdict,
                              const char* key,
                              int64_t value);
void xpc_release(xpc_object_t object);
bool xpc_dictionary_get_bool(xpc_object_t xdict, const char* key);
int64_t xpc_dictionary_get_int64(xpc_object_t xdict, const char* key);
const char* xpc_dictionary_get_string(xpc_object_t xdict, const char* key);
uint64_t xpc_dictionary_get_uint64(xpc_object_t xdict, const char* key);
void xpc_dictionary_set_uint64(xpc_object_t xdict,
                               const char* key,
                               uint64_t value);
void xpc_dictionary_set_string(xpc_object_t xdict, const char* key,
                               const char* string);
xpc_object_t xpc_dictionary_create(const char* const* keys,
                                   const xpc_object_t* values,
                                   size_t count);
xpc_object_t xpc_dictionary_create_reply(xpc_object_t original);
xpc_object_t xpc_dictionary_get_value(xpc_object_t xdict, const char* key);
char* xpc_copy_description(xpc_object_t object);
}  // extern "C"
#endif

// Signatures for private XPC functions.
extern "C" {
// Dictionary manipulation.
void xpc_dictionary_set_mach_send(xpc_object_t dictionary,
                                  const char* name,
                                  mach_port_t port);
void xpc_dictionary_get_audit_token(xpc_object_t dictionary,
                                    audit_token_t* token);

// Raw object getters.
mach_port_t xpc_mach_send_get_right(xpc_object_t value);

// Pipe methods.
xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, int flags);
int xpc_pipe_receive(mach_port_t port, xpc_object_t* message);
int xpc_pipe_routine(xpc_pipe_t pipe,
                     xpc_object_t request,
                     xpc_object_t* reply);
int xpc_pipe_routine_reply(xpc_object_t reply);
int xpc_pipe_simpleroutine(xpc_pipe_t pipe, xpc_object_t message);
int xpc_pipe_routine_forward(xpc_pipe_t forward_to, xpc_object_t request);
}  // extern "C"

#endif  // SANDBOX_MAC_XPC_H_
