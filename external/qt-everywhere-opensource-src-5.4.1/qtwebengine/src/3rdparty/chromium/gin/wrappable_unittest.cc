// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

class BaseClass {
 public:
  BaseClass() : value_(23) {}
  virtual ~BaseClass() {}

 private:
  int value_;

  DISALLOW_COPY_AND_ASSIGN(BaseClass);
};

class MyObject : public BaseClass,
                 public Wrappable<MyObject> {
 public:
  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyObject> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyObject());
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

 protected:
  MyObject() : value_(0) {}
  virtual ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;
  virtual ~MyObject() {}

 private:
  int value_;
};

class MyObjectSubclass : public MyObject {
 public:
  static gin::Handle<MyObjectSubclass> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyObjectSubclass());
  }

  void SayHello(const std::string& name) {
    result = std::string("Hello, ") + name;
  }

  std::string result;

 private:
  virtual ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE {
    return MyObject::GetObjectTemplateBuilder(isolate)
        .SetMethod("sayHello", &MyObjectSubclass::SayHello);
  }

  MyObjectSubclass() {
  }

  virtual ~MyObjectSubclass() {
  }
};

class MyCallableObject : public Wrappable<MyCallableObject> {
 public:
  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyCallableObject> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyCallableObject());
  }

  int result() { return result_; }

 private:
  virtual ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE {
    return Wrappable<MyCallableObject>::GetObjectTemplateBuilder(isolate)
        .SetCallAsFunctionHandler(&MyCallableObject::Call);
  }

  MyCallableObject() : result_(0) {
  }

  virtual ~MyCallableObject() {
  }

  void Call(int val) {
    result_ = val;
  }

  int result_;
};

class MyObject2 : public Wrappable<MyObject2> {
 public:
  static WrapperInfo kWrapperInfo;
};

class MyObjectBlink : public Wrappable<MyObjectBlink> {
 public:
  static WrapperInfo kWrapperInfo;
};

WrapperInfo MyObject::kWrapperInfo = { kEmbedderNativeGin };
ObjectTemplateBuilder MyObject::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return Wrappable<MyObject>::GetObjectTemplateBuilder(isolate)
      .SetProperty("value", &MyObject::value, &MyObject::set_value);
}

WrapperInfo MyCallableObject::kWrapperInfo = { kEmbedderNativeGin };
WrapperInfo MyObject2::kWrapperInfo = { kEmbedderNativeGin };
WrapperInfo MyObjectBlink::kWrapperInfo = { kEmbedderNativeGin };

typedef V8Test WrappableTest;

TEST_F(WrappableTest, WrapAndUnwrap) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  Handle<MyObject> obj = MyObject::Create(isolate);

  v8::Handle<v8::Value> wrapper = ConvertToV8(isolate, obj.get());
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = NULL;
  EXPECT_TRUE(ConvertFromV8(isolate, wrapper, &unwrapped));
  EXPECT_EQ(obj.get(), unwrapped);
}

TEST_F(WrappableTest, UnwrapFailures) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  // Something that isn't an object.
  v8::Handle<v8::Value> thing = v8::Number::New(isolate, 42);
  MyObject* unwrapped = NULL;
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's not wrapping anything.
  thing = v8::Object::New(isolate);
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's wrapping a C++ object from Blink.
  thing.Clear();
  thing = ConvertToV8(isolate, new MyObjectBlink());
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's wrapping a C++ object of the wrong type.
  thing.Clear();
  thing = ConvertToV8(isolate, new MyObject2());
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);
}

TEST_F(WrappableTest, GetAndSetProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  gin::Handle<MyObject> obj = MyObject::Create(isolate);

  obj->set_value(42);
  EXPECT_EQ(42, obj->value());

  v8::Handle<v8::String> source = StringToV8(isolate,
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
  EXPECT_FALSE(source.IsEmpty());

  gin::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  EXPECT_FALSE(script.IsEmpty());
  v8::Handle<v8::Value> val = script->Run();
  EXPECT_FALSE(val.IsEmpty());
  v8::Handle<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
  v8::Handle<v8::Value> argv[] = {
    ConvertToV8(isolate, obj.get()),
  };
  func->Call(v8::Undefined(isolate), 1, argv);
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_EQ("", try_catch.GetStackTrace());

  EXPECT_EQ(191, obj->value());
}

TEST_F(WrappableTest, WrappableSubclass) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  gin::Handle<MyObjectSubclass> object(MyObjectSubclass::Create(isolate));
  v8::Handle<v8::String> source = StringToV8(isolate,
                                             "(function(obj) {"
                                             "obj.sayHello('Lily');"
                                             "})");
  gin::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  v8::Handle<v8::Value> val = script->Run();
  v8::Handle<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
  v8::Handle<v8::Value> argv[] = {
    ConvertToV8(isolate, object.get())
  };
  func->Call(v8::Undefined(isolate), 1, argv);
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_EQ("Hello, Lily", object->result);
}

TEST_F(WrappableTest, ErrorInObjectConstructorProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::String> source = StringToV8(
      isolate,
      "(function() {"
      "  Object.defineProperty(Object.prototype, 'constructor', {"
      "    get: function() { throw 'Error'; },"
      "    set: function() { throw 'Error'; }"
      "  });"
      "})();");
  EXPECT_FALSE(source.IsEmpty());
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  script->Run();

  gin::TryCatch try_catch;
  gin::Handle<MyObject> obj = MyObject::Create(isolate);
  EXPECT_TRUE(obj.IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST_F(WrappableTest, CallAsFunction) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  gin::Handle<MyCallableObject> object(MyCallableObject::Create(isolate));
  EXPECT_EQ(0, object->result());
  v8::Handle<v8::String> source = StringToV8(isolate,
                                             "(function(obj) {"
                                             "obj(42);"
                                             "})");
  gin::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  v8::Handle<v8::Value> val = script->Run();
  v8::Handle<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
  v8::Handle<v8::Value> argv[] = {
    ConvertToV8(isolate, object.get())
  };
  func->Call(v8::Undefined(isolate), 1, argv);
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_EQ(42, object->result());
}

}  // namespace gin
