// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_param_traits.h"

#include <string>
#include <vector>
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "ipc/ipc_message_utils.h"

using content::IndexedDBKey;
using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;

using blink::WebIDBKeyPathTypeArray;
using blink::WebIDBKeyPathTypeNull;
using blink::WebIDBKeyPathTypeString;
using blink::WebIDBKeyType;
using blink::WebIDBKeyTypeArray;
using blink::WebIDBKeyTypeBinary;
using blink::WebIDBKeyTypeDate;
using blink::WebIDBKeyTypeInvalid;
using blink::WebIDBKeyTypeMin;
using blink::WebIDBKeyTypeNull;
using blink::WebIDBKeyTypeNumber;
using blink::WebIDBKeyTypeString;

namespace IPC {

void ParamTraits<IndexedDBKey>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case WebIDBKeyTypeArray:
      WriteParam(m, p.array());
      return;
    case WebIDBKeyTypeBinary:
      WriteParam(m, p.binary());
      return;
    case WebIDBKeyTypeString:
      WriteParam(m, p.string());
      return;
    case WebIDBKeyTypeDate:
      WriteParam(m, p.date());
      return;
    case WebIDBKeyTypeNumber:
      WriteParam(m, p.number());
      return;
    case WebIDBKeyTypeInvalid:
    case WebIDBKeyTypeNull:
      return;
    case WebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return;
  }
}

bool ParamTraits<IndexedDBKey>::Read(const Message* m,
                                     PickleIterator* iter,
                                     param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;
  WebIDBKeyType web_type = static_cast<WebIDBKeyType>(type);

  switch (web_type) {
    case WebIDBKeyTypeArray: {
      std::vector<IndexedDBKey> array;
      if (!ReadParam(m, iter, &array))
        return false;
      *r = IndexedDBKey(array);
      return true;
    }
    case WebIDBKeyTypeBinary: {
      std::string binary;
      if (!ReadParam(m, iter, &binary))
        return false;
      *r = IndexedDBKey(binary);
      return true;
    }
    case WebIDBKeyTypeString: {
      base::string16 string;
      if (!ReadParam(m, iter, &string))
        return false;
      *r = IndexedDBKey(string);
      return true;
    }
    case WebIDBKeyTypeDate:
    case WebIDBKeyTypeNumber: {
      double number;
      if (!ReadParam(m, iter, &number))
        return false;
      *r = IndexedDBKey(number, web_type);
      return true;
    }
    case WebIDBKeyTypeInvalid:
    case WebIDBKeyTypeNull:
      *r = IndexedDBKey(web_type);
      return true;
    case WebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return false;
  }
}

void ParamTraits<IndexedDBKey>::Log(const param_type& p, std::string* l) {
  l->append("<IndexedDBKey>(");
  LogParam(static_cast<int>(p.type()), l);
  l->append(", ");
  l->append("[");
  std::vector<IndexedDBKey>::const_iterator it = p.array().begin();
  while (it != p.array().end()) {
    Log(*it, l);
    ++it;
    if (it != p.array().end())
      l->append(", ");
  }
  l->append("], ");
  LogParam(p.binary(), l);
  l->append(", ");
  LogParam(p.string(), l);
  l->append(", ");
  LogParam(p.date(), l);
  l->append(", ");
  LogParam(p.number(), l);
  l->append(")");
}

void ParamTraits<IndexedDBKeyPath>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case WebIDBKeyPathTypeArray:
      WriteParam(m, p.array());
      return;
    case WebIDBKeyPathTypeString:
      WriteParam(m, p.string());
      return;
    case WebIDBKeyPathTypeNull:
      return;
    default:
      NOTREACHED();
      return;
  }
}

bool ParamTraits<IndexedDBKeyPath>::Read(const Message* m,
                                         PickleIterator* iter,
                                         param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  switch (type) {
    case WebIDBKeyPathTypeArray: {
      std::vector<base::string16> array;
      if (!ReadParam(m, iter, &array))
        return false;
      *r = IndexedDBKeyPath(array);
      return true;
    }
    case WebIDBKeyPathTypeString: {
      base::string16 string;
      if (!ReadParam(m, iter, &string))
        return false;
      *r = IndexedDBKeyPath(string);
      return true;
    }
    case WebIDBKeyPathTypeNull:
      *r = IndexedDBKeyPath();
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

void ParamTraits<IndexedDBKeyPath>::Log(const param_type& p, std::string* l) {
  l->append("<IndexedDBKeyPath>(");
  LogParam(static_cast<int>(p.type()), l);
  l->append(", ");
  LogParam(p.string(), l);
  l->append(", ");
  l->append("[");
  std::vector<base::string16>::const_iterator it = p.array().begin();
  while (it != p.array().end()) {
    LogParam(*it, l);
    ++it;
    if (it != p.array().end())
      l->append(", ");
  }
  l->append("])");
}

void ParamTraits<IndexedDBKeyRange>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.lower());
  WriteParam(m, p.upper());
  WriteParam(m, p.lowerOpen());
  WriteParam(m, p.upperOpen());
}

bool ParamTraits<IndexedDBKeyRange>::Read(const Message* m,
                                          PickleIterator* iter,
                                          param_type* r) {
  IndexedDBKey lower;
  if (!ReadParam(m, iter, &lower))
    return false;

  IndexedDBKey upper;
  if (!ReadParam(m, iter, &upper))
    return false;

  bool lower_open;
  if (!ReadParam(m, iter, &lower_open))
    return false;

  bool upper_open;
  if (!ReadParam(m, iter, &upper_open))
    return false;

  *r = IndexedDBKeyRange(lower, upper, lower_open, upper_open);
  return true;
}

void ParamTraits<IndexedDBKeyRange>::Log(const param_type& p, std::string* l) {
  l->append("<IndexedDBKeyRange>(lower=");
  LogParam(p.lower(), l);
  l->append(", upper=");
  LogParam(p.upper(), l);
  l->append(", lower_open=");
  LogParam(p.lowerOpen(), l);
  l->append(", upper_open=");
  LogParam(p.upperOpen(), l);
  l->append(")");
}

}  // namespace IPC
