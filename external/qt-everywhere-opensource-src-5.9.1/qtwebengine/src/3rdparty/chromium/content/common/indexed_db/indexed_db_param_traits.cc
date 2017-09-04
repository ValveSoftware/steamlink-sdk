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

void ParamTraits<IndexedDBKey>::GetSize(base::PickleSizer* s,
                                        const param_type& p) {
  GetParamSize(s, static_cast<int>(p.type()));
  switch (p.type()) {
    case WebIDBKeyTypeArray:
      GetParamSize(s, p.array());
      return;
    case WebIDBKeyTypeBinary:
      GetParamSize(s, p.binary());
      return;
    case WebIDBKeyTypeString:
      GetParamSize(s, p.string());
      return;
    case WebIDBKeyTypeDate:
      GetParamSize(s, p.date());
      return;
    case WebIDBKeyTypeNumber:
      GetParamSize(s, p.number());
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

void ParamTraits<IndexedDBKey>::Write(base::Pickle* m, const param_type& p) {
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

bool ParamTraits<IndexedDBKey>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
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
  switch(p.type()) {
    case WebIDBKeyTypeArray: {
      l->append("array=");
      l->append("[");
      bool first = true;
      for (const IndexedDBKey& key : p.array()) {
        if (!first)
          l->append(", ");
        first = false;
        Log(key, l);
      }
      l->append("]");
      break;
    }
    case WebIDBKeyTypeBinary:
      l->append("binary=");
      LogParam(p.binary(), l);
      break;
    case WebIDBKeyTypeString:
      l->append("string=");
      LogParam(p.string(), l);
      break;
    case WebIDBKeyTypeDate:
      l->append("date=");
      LogParam(p.date(), l);
      break;
    case WebIDBKeyTypeNumber:
      l->append("number=");
      LogParam(p.number(), l);
      break;
    case WebIDBKeyTypeInvalid:
      l->append("invalid");
      break;
    case WebIDBKeyTypeNull:
      l->append("null");
      break;
    case WebIDBKeyTypeMin:
    default:
      NOTREACHED();
      break;
  }
  l->append(")");
}

void ParamTraits<IndexedDBKeyPath>::GetSize(base::PickleSizer* s,
                                            const param_type& p) {
  GetParamSize(s, static_cast<int>(p.type()));
  switch (p.type()) {
    case WebIDBKeyPathTypeArray:
      GetParamSize(s, p.array());
      return;
    case WebIDBKeyPathTypeString:
      GetParamSize(s, p.string());
      return;
    case WebIDBKeyPathTypeNull:
      return;
    default:
      NOTREACHED();
      return;
  }
}

void ParamTraits<IndexedDBKeyPath>::Write(base::Pickle* m,
                                          const param_type& p) {
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

bool ParamTraits<IndexedDBKeyPath>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
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
  switch (p.type()) {
    case WebIDBKeyPathTypeArray: {
      l->append("array=[");
      bool first = true;
      for (const base::string16& entry : p.array()) {
        if (!first)
          l->append(", ");
        first = false;
        LogParam(entry, l);
      }
      l->append("]");
      break;
    }
    case WebIDBKeyPathTypeString:
      l->append("string=");
      LogParam(p.string(), l);
      break;
    case WebIDBKeyPathTypeNull:
      l->append("null");
      break;
    default:
      NOTREACHED();
      break;
  }
  l->append(")");
}

void ParamTraits<IndexedDBKeyRange>::GetSize(base::PickleSizer* s,
                                             const param_type& p) {
  GetParamSize(s, p.lower());
  GetParamSize(s, p.upper());
  GetParamSize(s, p.lower_open());
  GetParamSize(s, p.upper_open());
}

void ParamTraits<IndexedDBKeyRange>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteParam(m, p.lower());
  WriteParam(m, p.upper());
  WriteParam(m, p.lower_open());
  WriteParam(m, p.upper_open());
}

bool ParamTraits<IndexedDBKeyRange>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
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
  LogParam(p.lower_open(), l);
  l->append(", upper_open=");
  LogParam(p.upper_open(), l);
  l->append(")");
}

}  // namespace IPC
