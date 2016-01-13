// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains declarations of private XPC functions. This file is
// used for both forward declarations of private symbols and to use with
// tools/generate_stubs for creating a dynamic library loader.

// Dictionary manipulation.
void xpc_dictionary_set_mach_send(xpc_object_t dict, const char* name, mach_port_t port);

// Pipe methods.
xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, int flags);
int xpc_pipe_receive(mach_port_t port, xpc_object_t* message);
int xpc_pipe_routine(xpc_pipe_t pipe, xpc_object_t request, xpc_object_t* reply);
int xpc_pipe_routine_reply(xpc_object_t reply);
int xpc_pipe_routine_forward(xpc_pipe_t forward_to, xpc_object_t request);
