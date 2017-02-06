// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace ui {

class ClipboardAndroidTest : public ::testing::Test {
 public:
  ~ClipboardAndroidTest() override {
    Clipboard::DestroyClipboardForCurrentThread();
  };

 protected:
  Clipboard& clipboard() { return *Clipboard::GetForCurrentThread(); }
};

// Test that if another application writes some text to the pasteboard the
// clipboard properly invalidates other types.
TEST_F(ClipboardAndroidTest, InternalClipboardInvalidation) {
  // Write a Webkit smart paste tag to our clipboard.
  {
    ScopedClipboardWriter clipboard_writer(CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteWebSmartPaste();
  }
  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType(), CLIPBOARD_TYPE_COPY_PASTE));

  //
  // Simulate that another application copied something in the Clipboard
  //
  std::string new_value("Some text copied by some other app");
  using base::android::ConvertUTF8ToJavaString;
  using base::android::MethodID;
  using base::android::ScopedJavaLocalRef;

  JNIEnv* env = base::android::AttachCurrentThread();
  ASSERT_TRUE(env);

  jobject context = base::android::GetApplicationContext();
  ASSERT_TRUE(context);

  ScopedJavaLocalRef<jclass> context_class =
      base::android::GetClass(env, "android/content/Context");

  jmethodID get_system_service = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, context_class.obj(), "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;");

  // Retrieve the system service.
  ScopedJavaLocalRef<jstring> service_name =
      ConvertUTF8ToJavaString(env, "clipboard");
  ScopedJavaLocalRef<jobject> clipboard_manager(
      env,
      env->CallObjectMethod(context, get_system_service, service_name.obj()));
  ASSERT_TRUE(clipboard_manager.obj() && !base::android::ClearException(env));

  ScopedJavaLocalRef<jclass> clipboard_class =
      base::android::GetClass(env, "android/text/ClipboardManager");
  jmethodID set_text = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, clipboard_class.obj(), "setText", "(Ljava/lang/CharSequence;)V");
  ScopedJavaLocalRef<jstring> new_value_string =
      ConvertUTF8ToJavaString(env, new_value.c_str());

  // Will need to call toString as CharSequence is not always a String.
  env->CallVoidMethod(clipboard_manager.obj(), set_text,
                      new_value_string.obj());

  // The WebKit smart paste tag should now be gone.
  EXPECT_FALSE(clipboard().IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType(), CLIPBOARD_TYPE_COPY_PASTE));

  // Make sure some text is available
  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), CLIPBOARD_TYPE_COPY_PASTE));

  // Make sure the text is what we inserted while simulating the other app
  std::string contents;
  clipboard().ReadAsciiText(CLIPBOARD_TYPE_COPY_PASTE, &contents);
  EXPECT_EQ(contents, new_value);
}

}  // namespace ui

#include "ui/base/clipboard/clipboard_test_template.h"
