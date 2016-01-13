// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/plugin_param_traits.h"

#include "base/strings/string_number_conversions.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/public/web/WebBindings.h"

namespace content {

NPIdentifier_Param::NPIdentifier_Param()
    : identifier() {
}

NPIdentifier_Param::~NPIdentifier_Param() {
}

NPVariant_Param::NPVariant_Param()
    : type(NPVARIANT_PARAM_VOID),
      bool_value(false),
      int_value(0),
      double_value(0),
      npobject_routing_id(-1),
      npobject_owner_id(-1) {
}

NPVariant_Param::~NPVariant_Param() {
}

}  // namespace content

using content::NPIdentifier_Param;
using content::NPVariant_Param;

namespace IPC {

void ParamTraits<NPVariant_Param>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type));
  if (p.type == content::NPVARIANT_PARAM_BOOL) {
    WriteParam(m, p.bool_value);
  } else if (p.type == content::NPVARIANT_PARAM_INT) {
    WriteParam(m, p.int_value);
  } else if (p.type == content::NPVARIANT_PARAM_DOUBLE) {
    WriteParam(m, p.double_value);
  } else if (p.type == content::NPVARIANT_PARAM_STRING) {
    WriteParam(m, p.string_value);
  } else if (p.type == content::NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             p.type == content::NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    // This is the routing id used to connect NPObjectProxy in the other
    // process with NPObjectStub in this process or to identify the raw
    // npobject pointer to be used in the callee process.
    WriteParam(m, p.npobject_routing_id);
    // This is a routing Id used to identify the plugin instance that owns
    // the object, for ownership-tracking purposes.
    WriteParam(m, p.npobject_owner_id);
  } else {
    DCHECK(p.type == content::NPVARIANT_PARAM_VOID ||
           p.type == content::NPVARIANT_PARAM_NULL);
  }
}

bool ParamTraits<NPVariant_Param>::Read(const Message* m,
                                        PickleIterator* iter,
                                        param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  bool result = false;
  r->type = static_cast<content::NPVariant_ParamEnum>(type);
  if (r->type == content::NPVARIANT_PARAM_BOOL) {
    result = ReadParam(m, iter, &r->bool_value);
  } else if (r->type == content::NPVARIANT_PARAM_INT) {
    result = ReadParam(m, iter, &r->int_value);
  } else if (r->type == content::NPVARIANT_PARAM_DOUBLE) {
    result = ReadParam(m, iter, &r->double_value);
  } else if (r->type == content::NPVARIANT_PARAM_STRING) {
    result = ReadParam(m, iter, &r->string_value);
  } else if (r->type == content::NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             r->type == content::NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    result = ReadParam(m, iter, &r->npobject_routing_id) &&
        ReadParam(m, iter, &r->npobject_owner_id);
  } else if ((r->type == content::NPVARIANT_PARAM_VOID) ||
             (r->type == content::NPVARIANT_PARAM_NULL)) {
    result = true;
  } else {
    NOTREACHED();
  }

  return result;
}

void ParamTraits<NPVariant_Param>::Log(const param_type& p, std::string* l) {
  l->append(
      base::StringPrintf("NPVariant_Param(%d, ", static_cast<int>(p.type)));
  if (p.type == content::NPVARIANT_PARAM_BOOL) {
    LogParam(p.bool_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_INT) {
    LogParam(p.int_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_DOUBLE) {
    LogParam(p.double_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_STRING) {
    LogParam(p.string_value, l);
  } else if (p.type == content::NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID ||
             p.type == content::NPVARIANT_PARAM_RECEIVER_OBJECT_ROUTING_ID) {
    LogParam(p.npobject_routing_id, l);
  } else {
    l->append("<none>");
  }
  l->append(")");
}

void ParamTraits<NPIdentifier_Param>::Write(Message* m, const param_type& p) {
  content::SerializeNPIdentifier(p.identifier, m);
}

bool ParamTraits<NPIdentifier_Param>::Read(const Message* m,
                                           PickleIterator* iter,
                                           param_type* r) {
  return content::DeserializeNPIdentifier(iter, &r->identifier);
}

void ParamTraits<NPIdentifier_Param>::Log(const param_type& p, std::string* l) {
  if (blink::WebBindings::identifierIsString(p.identifier)) {
    NPUTF8* str = blink::WebBindings::utf8FromIdentifier(p.identifier);
    l->append(str);
    free(str);
  } else {
    l->append(base::IntToString(
        blink::WebBindings::intFromIdentifier(p.identifier)));
  }
}

}  // namespace IPC
