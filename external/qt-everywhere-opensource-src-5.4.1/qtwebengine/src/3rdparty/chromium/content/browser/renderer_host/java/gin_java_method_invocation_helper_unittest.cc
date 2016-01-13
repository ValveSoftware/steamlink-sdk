// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/gin_java_method_invocation_helper.h"

#include "base/android/jni_android.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class NullObjectDelegate
    : public GinJavaMethodInvocationHelper::ObjectDelegate {
 public:
  NullObjectDelegate() {}

  virtual ~NullObjectDelegate() {}

  virtual base::android::ScopedJavaLocalRef<jobject> GetLocalRef(
      JNIEnv* env) OVERRIDE {
    return base::android::ScopedJavaLocalRef<jobject>();
  }

  virtual base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(
      JNIEnv* env) OVERRIDE {
    return base::android::ScopedJavaLocalRef<jclass>();
  }

  virtual const JavaMethod* FindMethod(const std::string& method_name,
                                       size_t num_parameters) OVERRIDE {
    return NULL;
  }

  virtual bool IsObjectGetClassMethod(const JavaMethod* method) OVERRIDE {
    return false;
  }

  virtual const base::android::JavaRef<jclass>& GetSafeAnnotationClass()
      OVERRIDE {
    return safe_annotation_class_;
  }

 private:
  base::android::ScopedJavaLocalRef<jclass> safe_annotation_class_;

  DISALLOW_COPY_AND_ASSIGN(NullObjectDelegate);
};

class NullDispatcherDelegate
    : public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  NullDispatcherDelegate() {}

  virtual ~NullDispatcherDelegate() {}

  virtual JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) OVERRIDE {
    return JavaObjectWeakGlobalRef();
  }

  DISALLOW_COPY_AND_ASSIGN(NullDispatcherDelegate);
};

}  // namespace

class GinJavaMethodInvocationHelperTest : public testing::Test {
};

namespace {

class CountingDispatcherDelegate
    : public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  CountingDispatcherDelegate() {}

  virtual ~CountingDispatcherDelegate() {}

  virtual JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) OVERRIDE {
    counters_[object_id]++;
    return JavaObjectWeakGlobalRef();
  }

  void AssertInvocationsCount(GinJavaBoundObject::ObjectID begin_object_id,
                              GinJavaBoundObject::ObjectID end_object_id) {
    EXPECT_EQ(end_object_id - begin_object_id,
              static_cast<int>(counters_.size()));
    for (GinJavaBoundObject::ObjectID i = begin_object_id;
         i < end_object_id; ++i) {
      EXPECT_LT(0, counters_[i]) << "ObjectID: " << i;
    }
  }

 private:
  typedef std::map<GinJavaBoundObject::ObjectID, int> Counters;
  Counters counters_;

  DISALLOW_COPY_AND_ASSIGN(CountingDispatcherDelegate);
};

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, RetrievalOfObjectsNoObjects) {
  base::ListValue no_objects;
  for (int i = 0; i < 10; ++i) {
    no_objects.AppendInteger(i);
  }

  scoped_refptr<GinJavaMethodInvocationHelper> helper =
      new GinJavaMethodInvocationHelper(
          scoped_ptr<GinJavaMethodInvocationHelper::ObjectDelegate>(
              new NullObjectDelegate()),
          "foo",
          no_objects);
  CountingDispatcherDelegate counter;
  helper->Init(&counter);
  counter.AssertInvocationsCount(0, 0);
}

TEST_F(GinJavaMethodInvocationHelperTest, RetrievalOfObjectsHaveObjects) {
  base::ListValue objects;
  objects.AppendInteger(100);
  objects.Append(GinJavaBridgeValue::CreateObjectIDValue(1).release());
  base::ListValue* sub_list = new base::ListValue();
  sub_list->AppendInteger(200);
  sub_list->Append(GinJavaBridgeValue::CreateObjectIDValue(2).release());
  objects.Append(sub_list);
  base::DictionaryValue* sub_dict = new base::DictionaryValue();
  sub_dict->SetInteger("1", 300);
  sub_dict->Set("2", GinJavaBridgeValue::CreateObjectIDValue(3).release());
  objects.Append(sub_dict);
  base::ListValue* sub_list_with_dict = new base::ListValue();
  base::DictionaryValue* sub_sub_dict = new base::DictionaryValue();
  sub_sub_dict->Set("1", GinJavaBridgeValue::CreateObjectIDValue(4).release());
  sub_list_with_dict->Append(sub_sub_dict);
  objects.Append(sub_list_with_dict);
  base::DictionaryValue* sub_dict_with_list = new base::DictionaryValue();
  base::ListValue* sub_sub_list = new base::ListValue();
  sub_sub_list->Append(GinJavaBridgeValue::CreateObjectIDValue(5).release());
  sub_dict_with_list->Set("1", sub_sub_list);
  objects.Append(sub_dict_with_list);

  scoped_refptr<GinJavaMethodInvocationHelper> helper =
      new GinJavaMethodInvocationHelper(
          scoped_ptr<GinJavaMethodInvocationHelper::ObjectDelegate>(
              new NullObjectDelegate()),
          "foo",
          objects);
  CountingDispatcherDelegate counter;
  helper->Init(&counter);
  counter.AssertInvocationsCount(1, 6);
}

TEST_F(GinJavaMethodInvocationHelperTest, HandleObjectIsGone) {
  base::ListValue no_objects;
  scoped_refptr<GinJavaMethodInvocationHelper> helper =
      new GinJavaMethodInvocationHelper(
          scoped_ptr<GinJavaMethodInvocationHelper::ObjectDelegate>(
              new NullObjectDelegate()),
          "foo",
          no_objects);
  NullDispatcherDelegate dispatcher;
  helper->Init(&dispatcher);
  EXPECT_TRUE(helper->GetErrorMessage().empty());
  helper->Invoke();
  EXPECT_TRUE(helper->HoldsPrimitiveResult());
  EXPECT_TRUE(helper->GetPrimitiveResult().empty());
  EXPECT_FALSE(helper->GetErrorMessage().empty());
}

namespace {

class MethodNotFoundObjectDelegate : public NullObjectDelegate {
 public:
  MethodNotFoundObjectDelegate() : find_method_called_(false) {}

  virtual ~MethodNotFoundObjectDelegate() {}

  virtual base::android::ScopedJavaLocalRef<jobject> GetLocalRef(
      JNIEnv* env) OVERRIDE {
    return base::android::ScopedJavaLocalRef<jobject>(
        env, static_cast<jobject>(env->FindClass("java/lang/String")));
  }

  virtual const JavaMethod* FindMethod(const std::string& method_name,
                                       size_t num_parameters) OVERRIDE {
    find_method_called_ = true;
    return NULL;
  }

  bool find_method_called() const { return find_method_called_; }

 protected:
  bool find_method_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodNotFoundObjectDelegate);
};

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, HandleMethodNotFound) {
  base::ListValue no_objects;
  MethodNotFoundObjectDelegate* object_delegate =
      new MethodNotFoundObjectDelegate();
  scoped_refptr<GinJavaMethodInvocationHelper> helper =
      new GinJavaMethodInvocationHelper(
          scoped_ptr<GinJavaMethodInvocationHelper::ObjectDelegate>(
              object_delegate),
          "foo",
          no_objects);
  NullDispatcherDelegate dispatcher;
  helper->Init(&dispatcher);
  EXPECT_FALSE(object_delegate->find_method_called());
  EXPECT_TRUE(helper->GetErrorMessage().empty());
  helper->Invoke();
  EXPECT_TRUE(object_delegate->find_method_called());
  EXPECT_TRUE(helper->HoldsPrimitiveResult());
  EXPECT_TRUE(helper->GetPrimitiveResult().empty());
  EXPECT_FALSE(helper->GetErrorMessage().empty());
}

namespace {

class GetClassObjectDelegate : public MethodNotFoundObjectDelegate {
 public:
  GetClassObjectDelegate() : get_class_called_(false) {}

  virtual ~GetClassObjectDelegate() {}

  virtual const JavaMethod* FindMethod(const std::string& method_name,
                                       size_t num_parameters) OVERRIDE {
    find_method_called_ = true;
    return kFakeGetClass;
  }

  virtual bool IsObjectGetClassMethod(const JavaMethod* method) OVERRIDE {
    get_class_called_ = true;
    return kFakeGetClass == method;
  }

  bool get_class_called() const { return get_class_called_; }

 private:
  static const JavaMethod* kFakeGetClass;
  bool get_class_called_;

  DISALLOW_COPY_AND_ASSIGN(GetClassObjectDelegate);
};

// We don't expect GinJavaMethodInvocationHelper to actually invoke the
// method, since the point of the test is to verify whether calls to
// 'getClass' get blocked.
const JavaMethod* GetClassObjectDelegate::kFakeGetClass =
    (JavaMethod*)0xdeadbeef;

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, HandleGetClassInvocation) {
  base::ListValue no_objects;
  GetClassObjectDelegate* object_delegate =
      new GetClassObjectDelegate();
  scoped_refptr<GinJavaMethodInvocationHelper> helper =
      new GinJavaMethodInvocationHelper(
          scoped_ptr<GinJavaMethodInvocationHelper::ObjectDelegate>(
              object_delegate),
          "foo",
          no_objects);
  NullDispatcherDelegate dispatcher;
  helper->Init(&dispatcher);
  EXPECT_FALSE(object_delegate->find_method_called());
  EXPECT_FALSE(object_delegate->get_class_called());
  EXPECT_TRUE(helper->GetErrorMessage().empty());
  helper->Invoke();
  EXPECT_TRUE(object_delegate->find_method_called());
  EXPECT_TRUE(object_delegate->get_class_called());
  EXPECT_TRUE(helper->HoldsPrimitiveResult());
  EXPECT_TRUE(helper->GetPrimitiveResult().empty());
  EXPECT_FALSE(helper->GetErrorMessage().empty());
}

}  // namespace content
