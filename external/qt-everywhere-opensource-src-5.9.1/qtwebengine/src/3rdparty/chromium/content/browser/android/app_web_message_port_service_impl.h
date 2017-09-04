// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_SERVICE_IMPL_H_
#define CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_SERVICE_IMPL_H_

#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "content/public/browser/android/app_web_message_port_service.h"

namespace content {
class AppWebMessagePortMessageFilter;

// This class is the native peer of AppWebMessagePortService.java.
// Please see the java class for an explanation of use, ownership and lifetime.

// Threading: Created and initialized on UI thread. For other methods, see
// the method level DCHECKS or documentation.
class AppWebMessagePortServiceImpl : public AppWebMessagePortService {
 public:
  // Returns the AppWebMessagePortServiceImpl singleton.
  static AppWebMessagePortServiceImpl* GetInstance();

  AppWebMessagePortServiceImpl();
  ~AppWebMessagePortServiceImpl() override;
  void Init(JNIEnv* env, jobject object);

  // AppWebMessagePortService implementation

  void CreateMessageChannel(JNIEnv* env,
                            jobjectArray ports,
                            WebContents* web_contents) override;
  void CleanupPort(int message_port_id) override;

  // Methods called from Java.
  void PostAppToWebMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      int sender_id,
      const base::android::JavaParamRef<jstring>& message,
      const base::android::JavaParamRef<jintArray>& sent_ports);
  void ClosePort(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& object,
                 int message_port_id);
  void ReleaseMessages(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& object,
                       int message_port_id);

  void OnMessagePortMessageFilterClosing(
      AppWebMessagePortMessageFilter* filter);

  // AppWebMessagePortServiceImpl specific calls
  void OnConvertedWebToAppMessage(
      int message_port_id,
      const base::ListValue& message,
      const std::vector<int>& sent_message_port_ids);
  void RemoveSentPorts(const std::vector<int>& sent_ports);

 private:
  friend struct base::DefaultSingletonTraits<AppWebMessagePortServiceImpl>;

  void PostAppToWebMessageOnIOThread(int sender_id,
                                     base::string16* message,
                                     std::vector<int>* sent_ports);
  void CreateMessageChannelOnIOThread(
      scoped_refptr<AppWebMessagePortMessageFilter> filter,
      int* port1,
      int* port2);
  void OnMessageChannelCreated(
      base::android::ScopedJavaGlobalRef<jobjectArray>* ports,
      int* port1,
      int* port2);
  void AddPort(int message_port_id, AppWebMessagePortMessageFilter* filter);
  void PostClosePortMessage(int message_port_id);

  JavaObjectWeakGlobalRef java_ref_;
  typedef std::map<int, AppWebMessagePortMessageFilter*> MessagePorts;
  MessagePorts ports_;  // Access on IO thread

  DISALLOW_COPY_AND_ASSIGN(AppWebMessagePortServiceImpl);
};

bool RegisterAppWebMessagePortService(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_SERVICE_IMPL_H_
