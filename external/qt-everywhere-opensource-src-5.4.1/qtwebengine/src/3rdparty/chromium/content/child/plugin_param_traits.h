// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are used by the content code, and which need
// manual serialization code.  This is usually because they're not structs with
// public members, or because the same type is being used in multiple
// *_messages.h headers.

#ifndef CONTENT_CHILD_PLUGIN_PARAM_TRAITS_H_
#define CONTENT_CHILD_PLUGIN_PARAM_TRAITS_H_

#include <string>

#include "content/child/npapi/npruntime_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"

namespace content {

// Define the NPVariant_Param struct and its enum here since it needs manual
// serialization code.
enum NPVariant_ParamEnum {
  NPVARIANT_PARAM_VOID,
  NPVARIANT_PARAM_NULL,
  NPVARIANT_PARAM_BOOL,
  NPVARIANT_PARAM_INT,
  NPVARIANT_PARAM_DOUBLE,
  NPVARIANT_PARAM_STRING,
  // Used when when the NPObject is running in the caller's process, so we
  // create an NPObjectProxy in the other process. To support object ownership
  // tracking the routing-Id of the NPObject's owning plugin instance is
  // passed alongside that of the object itself.
  NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID,
  // Used when the NPObject we're sending is running in the callee's process
  // (i.e. we have an NPObjectProxy for it).  In that case we want the callee
  // to just use the raw pointer.
  NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID,
};

struct NPVariant_Param {
  NPVariant_Param();
  ~NPVariant_Param();

  NPVariant_ParamEnum type;
  bool bool_value;
  int int_value;
  double double_value;
  std::string string_value;
  int npobject_routing_id;
  int npobject_owner_id;
};

struct NPIdentifier_Param {
  NPIdentifier_Param();
  ~NPIdentifier_Param();

  NPIdentifier identifier;
};

}  // namespace content

namespace IPC {

template <>
struct ParamTraits<content::NPVariant_Param> {
  typedef content::NPVariant_Param param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<content::NPIdentifier_Param> {
  typedef content::NPIdentifier_Param param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_CHILD_PLUGIN_PARAM_TRAITS_H_
