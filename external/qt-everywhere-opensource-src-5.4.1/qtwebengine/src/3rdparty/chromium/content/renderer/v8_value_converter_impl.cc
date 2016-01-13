// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/v8_value_converter_impl.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/float_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/web/WebArrayBufferConverter.h"
#include "third_party/WebKit/public/web/WebArrayBufferView.h"
#include "v8/include/v8.h"

namespace content {

// Default implementation of V8ValueConverter::Strategy

bool V8ValueConverter::Strategy::FromV8Object(
    v8::Handle<v8::Object> value,
    base::Value** out,
    v8::Isolate* isolate,
    const FromV8ValueCallback& callback) const {
  return false;
}

bool V8ValueConverter::Strategy::FromV8Array(
    v8::Handle<v8::Array> value,
    base::Value** out,
    v8::Isolate* isolate,
    const FromV8ValueCallback& callback) const {
  return false;
}

bool V8ValueConverter::Strategy::FromV8ArrayBuffer(v8::Handle<v8::Object> value,
                                                   base::Value** out,
                                                   v8::Isolate* isolate) const {
  return false;
}

bool V8ValueConverter::Strategy::FromV8Number(v8::Handle<v8::Number> value,
                                              base::Value** out) const {
  return false;
}

bool V8ValueConverter::Strategy::FromV8Undefined(base::Value** out) const {
  return false;
}


namespace {

// For the sake of the storage API, make this quite large.
const int kMaxRecursionDepth = 100;

}  // namespace

// The state of a call to FromV8Value.
class V8ValueConverterImpl::FromV8ValueState {
 public:
  // Level scope which updates the current depth of some FromV8ValueState.
  class Level {
   public:
    explicit Level(FromV8ValueState* state) : state_(state) {
      state_->max_recursion_depth_--;
    }
    ~Level() {
      state_->max_recursion_depth_++;
    }

   private:
    FromV8ValueState* state_;
  };

  explicit FromV8ValueState(bool avoid_identity_hash_for_testing)
      : max_recursion_depth_(kMaxRecursionDepth),
        avoid_identity_hash_for_testing_(avoid_identity_hash_for_testing) {}

  // If |handle| is not in |unique_map_|, then add it to |unique_map_| and
  // return true.
  //
  // Otherwise do nothing and return false. Here "A is unique" means that no
  // other handle B in the map points to the same object as A. Note that A can
  // be unique even if there already is another handle with the same identity
  // hash (key) in the map, because two objects can have the same hash.
  bool UpdateAndCheckUniqueness(v8::Handle<v8::Object> handle) {
    typedef HashToHandleMap::const_iterator Iterator;
    int hash = avoid_identity_hash_for_testing_ ? 0 : handle->GetIdentityHash();
    // We only compare using == with handles to objects with the same identity
    // hash. Different hash obviously means different objects, but two objects
    // in a couple of thousands could have the same identity hash.
    std::pair<Iterator, Iterator> range = unique_map_.equal_range(hash);
    for (Iterator it = range.first; it != range.second; ++it) {
      // Operator == for handles actually compares the underlying objects.
      if (it->second == handle)
        return false;
    }
    unique_map_.insert(std::make_pair(hash, handle));
    return true;
  }

  bool HasReachedMaxRecursionDepth() {
    return max_recursion_depth_ < 0;
  }

 private:
  typedef std::multimap<int, v8::Handle<v8::Object> > HashToHandleMap;
  HashToHandleMap unique_map_;

  int max_recursion_depth_;

  bool avoid_identity_hash_for_testing_;
};

V8ValueConverter* V8ValueConverter::create() {
  return new V8ValueConverterImpl();
}

V8ValueConverterImpl::V8ValueConverterImpl()
    : date_allowed_(false),
      reg_exp_allowed_(false),
      function_allowed_(false),
      strip_null_from_objects_(false),
      avoid_identity_hash_for_testing_(false),
      strategy_(NULL) {}

void V8ValueConverterImpl::SetDateAllowed(bool val) {
  date_allowed_ = val;
}

void V8ValueConverterImpl::SetRegExpAllowed(bool val) {
  reg_exp_allowed_ = val;
}

void V8ValueConverterImpl::SetFunctionAllowed(bool val) {
  function_allowed_ = val;
}

void V8ValueConverterImpl::SetStripNullFromObjects(bool val) {
  strip_null_from_objects_ = val;
}

void V8ValueConverterImpl::SetStrategy(Strategy* strategy) {
  strategy_ = strategy;
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8Value(
    const base::Value* value, v8::Handle<v8::Context> context) const {
  v8::Context::Scope context_scope(context);
  v8::EscapableHandleScope handle_scope(context->GetIsolate());
  return handle_scope.Escape(
      ToV8ValueImpl(context->GetIsolate(), context->Global(), value));
}

base::Value* V8ValueConverterImpl::FromV8Value(
    v8::Handle<v8::Value> val,
    v8::Handle<v8::Context> context) const {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(context->GetIsolate());
  FromV8ValueState state(avoid_identity_hash_for_testing_);
  return FromV8ValueImpl(&state, val, context->GetIsolate());
}

v8::Local<v8::Value> V8ValueConverterImpl::ToV8ValueImpl(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> creation_context,
    const base::Value* value) const {
  CHECK(value);
  switch (value->GetType()) {
    case base::Value::TYPE_NULL:
      return v8::Null(isolate);

    case base::Value::TYPE_BOOLEAN: {
      bool val = false;
      CHECK(value->GetAsBoolean(&val));
      return v8::Boolean::New(isolate, val);
    }

    case base::Value::TYPE_INTEGER: {
      int val = 0;
      CHECK(value->GetAsInteger(&val));
      return v8::Integer::New(isolate, val);
    }

    case base::Value::TYPE_DOUBLE: {
      double val = 0.0;
      CHECK(value->GetAsDouble(&val));
      return v8::Number::New(isolate, val);
    }

    case base::Value::TYPE_STRING: {
      std::string val;
      CHECK(value->GetAsString(&val));
      return v8::String::NewFromUtf8(
          isolate, val.c_str(), v8::String::kNormalString, val.length());
    }

    case base::Value::TYPE_LIST:
      return ToV8Array(isolate,
                       creation_context,
                       static_cast<const base::ListValue*>(value));

    case base::Value::TYPE_DICTIONARY:
      return ToV8Object(isolate,
                        creation_context,
                        static_cast<const base::DictionaryValue*>(value));

    case base::Value::TYPE_BINARY:
      return ToArrayBuffer(isolate,
                           creation_context,
                           static_cast<const base::BinaryValue*>(value));

    default:
      LOG(ERROR) << "Unexpected value type: " << value->GetType();
      return v8::Null(isolate);
  }
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8Array(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> creation_context,
    const base::ListValue* val) const {
  v8::Handle<v8::Array> result(v8::Array::New(isolate, val->GetSize()));

  for (size_t i = 0; i < val->GetSize(); ++i) {
    const base::Value* child = NULL;
    CHECK(val->Get(i, &child));

    v8::Handle<v8::Value> child_v8 =
        ToV8ValueImpl(isolate, creation_context, child);
    CHECK(!child_v8.IsEmpty());

    v8::TryCatch try_catch;
    result->Set(static_cast<uint32>(i), child_v8);
    if (try_catch.HasCaught())
      LOG(ERROR) << "Setter for index " << i << " threw an exception.";
  }

  return result;
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8Object(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> creation_context,
    const base::DictionaryValue* val) const {
  v8::Handle<v8::Object> result(v8::Object::New(isolate));

  for (base::DictionaryValue::Iterator iter(*val);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& key = iter.key();
    v8::Handle<v8::Value> child_v8 =
        ToV8ValueImpl(isolate, creation_context, &iter.value());
    CHECK(!child_v8.IsEmpty());

    v8::TryCatch try_catch;
    result->Set(
        v8::String::NewFromUtf8(
            isolate, key.c_str(), v8::String::kNormalString, key.length()),
        child_v8);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Setter for property " << key.c_str() << " threw an "
                 << "exception.";
    }
  }

  return result;
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToArrayBuffer(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> creation_context,
    const base::BinaryValue* value) const {
  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::create(value->GetSize(), 1);
  memcpy(buffer.data(), value->GetBuffer(), value->GetSize());
  return blink::WebArrayBufferConverter::toV8Value(
      &buffer, creation_context, isolate);
}

base::Value* V8ValueConverterImpl::FromV8ValueImpl(
    FromV8ValueState* state,
    v8::Handle<v8::Value> val,
    v8::Isolate* isolate) const {
  CHECK(!val.IsEmpty());

  FromV8ValueState::Level state_level(state);
  if (state->HasReachedMaxRecursionDepth())
    return NULL;

  if (val->IsNull())
    return base::Value::CreateNullValue();

  if (val->IsBoolean())
    return new base::FundamentalValue(val->ToBoolean()->Value());

  if (val->IsNumber() && strategy_) {
    base::Value* out = NULL;
    if (strategy_->FromV8Number(val->ToNumber(), &out))
      return out;
  }

  if (val->IsInt32())
    return new base::FundamentalValue(val->ToInt32()->Value());

  if (val->IsNumber()) {
    double val_as_double = val->ToNumber()->Value();
    if (!base::IsFinite(val_as_double))
      return NULL;
    return new base::FundamentalValue(val_as_double);
  }

  if (val->IsString()) {
    v8::String::Utf8Value utf8(val->ToString());
    return new base::StringValue(std::string(*utf8, utf8.length()));
  }

  if (val->IsUndefined()) {
    if (strategy_) {
      base::Value* out = NULL;
      if (strategy_->FromV8Undefined(&out))
        return out;
    }
    // JSON.stringify ignores undefined.
    return NULL;
  }

  if (val->IsDate()) {
    if (!date_allowed_)
      // JSON.stringify would convert this to a string, but an object is more
      // consistent within this class.
      return FromV8Object(val->ToObject(), state, isolate);
    v8::Date* date = v8::Date::Cast(*val);
    return new base::FundamentalValue(date->ValueOf() / 1000.0);
  }

  if (val->IsRegExp()) {
    if (!reg_exp_allowed_)
      // JSON.stringify converts to an object.
      return FromV8Object(val->ToObject(), state, isolate);
    return new base::StringValue(*v8::String::Utf8Value(val->ToString()));
  }

  // v8::Value doesn't have a ToArray() method for some reason.
  if (val->IsArray())
    return FromV8Array(val.As<v8::Array>(), state, isolate);

  if (val->IsFunction()) {
    if (!function_allowed_)
      // JSON.stringify refuses to convert function(){}.
      return NULL;
    return FromV8Object(val->ToObject(), state, isolate);
  }

  if (val->IsArrayBuffer() || val->IsArrayBufferView())
    return FromV8ArrayBuffer(val->ToObject(), isolate);

  if (val->IsObject())
    return FromV8Object(val->ToObject(), state, isolate);

  LOG(ERROR) << "Unexpected v8 value type encountered.";
  return NULL;
}

base::Value* V8ValueConverterImpl::FromV8Array(
    v8::Handle<v8::Array> val,
    FromV8ValueState* state,
    v8::Isolate* isolate) const {
  if (!state->UpdateAndCheckUniqueness(val))
    return base::Value::CreateNullValue();

  scoped_ptr<v8::Context::Scope> scope;
  // If val was created in a different context than our current one, change to
  // that context, but change back after val is converted.
  if (!val->CreationContext().IsEmpty() &&
      val->CreationContext() != isolate->GetCurrentContext())
    scope.reset(new v8::Context::Scope(val->CreationContext()));

  if (strategy_) {
    // These base::Unretained's are safe, because Strategy::FromV8Value should
    // be synchronous, so this object can't be out of scope.
    V8ValueConverter::Strategy::FromV8ValueCallback callback =
        base::Bind(&V8ValueConverterImpl::FromV8ValueImpl,
                   base::Unretained(this),
                   base::Unretained(state));
    base::Value* out = NULL;
    if (strategy_->FromV8Array(val, &out, isolate, callback))
      return out;
  }

  base::ListValue* result = new base::ListValue();

  // Only fields with integer keys are carried over to the ListValue.
  for (uint32 i = 0; i < val->Length(); ++i) {
    v8::TryCatch try_catch;
    v8::Handle<v8::Value> child_v8 = val->Get(i);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Getter for index " << i << " threw an exception.";
      child_v8 = v8::Null(isolate);
    }

    if (!val->HasRealIndexedProperty(i)) {
      result->Append(base::Value::CreateNullValue());
      continue;
    }

    base::Value* child = FromV8ValueImpl(state, child_v8, isolate);
    if (child)
      result->Append(child);
    else
      // JSON.stringify puts null in places where values don't serialize, for
      // example undefined and functions. Emulate that behavior.
      result->Append(base::Value::CreateNullValue());
  }
  return result;
}

base::Value* V8ValueConverterImpl::FromV8ArrayBuffer(
    v8::Handle<v8::Object> val,
    v8::Isolate* isolate) const {
  if (strategy_) {
    base::Value* out = NULL;
    if (strategy_->FromV8ArrayBuffer(val, &out, isolate))
      return out;
  }

  char* data = NULL;
  size_t length = 0;

  scoped_ptr<blink::WebArrayBuffer> array_buffer(
      blink::WebArrayBufferConverter::createFromV8Value(val, isolate));
  scoped_ptr<blink::WebArrayBufferView> view;
  if (array_buffer) {
    data = reinterpret_cast<char*>(array_buffer->data());
    length = array_buffer->byteLength();
  } else {
    view.reset(blink::WebArrayBufferView::createFromV8Value(val));
    if (view) {
      data = reinterpret_cast<char*>(view->baseAddress()) + view->byteOffset();
      length = view->byteLength();
    }
  }

  if (data)
    return base::BinaryValue::CreateWithCopiedBuffer(data, length);
  else
    return NULL;
}

base::Value* V8ValueConverterImpl::FromV8Object(
    v8::Handle<v8::Object> val,
    FromV8ValueState* state,
    v8::Isolate* isolate) const {
  if (!state->UpdateAndCheckUniqueness(val))
    return base::Value::CreateNullValue();

  scoped_ptr<v8::Context::Scope> scope;
  // If val was created in a different context than our current one, change to
  // that context, but change back after val is converted.
  if (!val->CreationContext().IsEmpty() &&
      val->CreationContext() != isolate->GetCurrentContext())
    scope.reset(new v8::Context::Scope(val->CreationContext()));

  if (strategy_) {
    // These base::Unretained's are safe, because Strategy::FromV8Value should
    // be synchronous, so this object can't be out of scope.
    V8ValueConverter::Strategy::FromV8ValueCallback callback =
        base::Bind(&V8ValueConverterImpl::FromV8ValueImpl,
                   base::Unretained(this),
                   base::Unretained(state));
    base::Value* out = NULL;
    if (strategy_->FromV8Object(val, &out, isolate, callback))
      return out;
  }

  // Don't consider DOM objects. This check matches isHostObject() in Blink's
  // bindings/v8/V8Binding.h used in structured cloning. It reads:
  //
  // If the object has any internal fields, then we won't be able to serialize
  // or deserialize them; conveniently, this is also a quick way to detect DOM
  // wrapper objects, because the mechanism for these relies on data stored in
  // these fields.
  //
  // NOTE: check this after |strategy_| so that callers have a chance to
  // do something else, such as convert to the node's name rather than NULL.
  //
  // ANOTHER NOTE: returning an empty dictionary here to minimise surprise.
  // See also http://crbug.com/330559.
  if (val->InternalFieldCount())
    return new base::DictionaryValue();

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  v8::Handle<v8::Array> property_names(val->GetOwnPropertyNames());

  for (uint32 i = 0; i < property_names->Length(); ++i) {
    v8::Handle<v8::Value> key(property_names->Get(i));

    // Extend this test to cover more types as necessary and if sensible.
    if (!key->IsString() &&
        !key->IsNumber()) {
      NOTREACHED() << "Key \"" << *v8::String::Utf8Value(key) << "\" "
                      "is neither a string nor a number";
      continue;
    }

    v8::String::Utf8Value name_utf8(key->ToString());

    v8::TryCatch try_catch;
    v8::Handle<v8::Value> child_v8 = val->Get(key);

    if (try_catch.HasCaught()) {
      LOG(WARNING) << "Getter for property " << *name_utf8
                   << " threw an exception.";
      child_v8 = v8::Null(isolate);
    }

    scoped_ptr<base::Value> child(FromV8ValueImpl(state, child_v8, isolate));
    if (!child)
      // JSON.stringify skips properties whose values don't serialize, for
      // example undefined and functions. Emulate that behavior.
      continue;

    // Strip null if asked (and since undefined is turned into null, undefined
    // too). The use case for supporting this is JSON-schema support,
    // specifically for extensions, where "optional" JSON properties may be
    // represented as null, yet due to buggy legacy code elsewhere isn't
    // treated as such (potentially causing crashes). For example, the
    // "tabs.create" function takes an object as its first argument with an
    // optional "windowId" property.
    //
    // Given just
    //
    //   tabs.create({})
    //
    // this will work as expected on code that only checks for the existence of
    // a "windowId" property (such as that legacy code). However given
    //
    //   tabs.create({windowId: null})
    //
    // there *is* a "windowId" property, but since it should be an int, code
    // on the browser which doesn't additionally check for null will fail.
    // We can avoid all bugs related to this by stripping null.
    if (strip_null_from_objects_ && child->IsType(base::Value::TYPE_NULL))
      continue;

    result->SetWithoutPathExpansion(std::string(*name_utf8, name_utf8.length()),
                                    child.release());
  }

  return result.release();
}

}  // namespace content
