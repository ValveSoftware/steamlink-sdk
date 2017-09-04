// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/app_web_message_port_service_impl.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "content/browser/android/app_web_message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "jni/AppWebMessagePortService_jni.h"

namespace content {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using content::BrowserThread;
using content::MessagePortProvider;

AppWebMessagePortServiceImpl* AppWebMessagePortServiceImpl::GetInstance() {
  return base::Singleton<AppWebMessagePortServiceImpl>::get();
}

AppWebMessagePortServiceImpl::AppWebMessagePortServiceImpl() {}

AppWebMessagePortServiceImpl::~AppWebMessagePortServiceImpl() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AppWebMessagePortService_unregisterNativeAppWebMessagePortService(env,
                                                                         obj);
}

void AppWebMessagePortServiceImpl::Init(JNIEnv* env, jobject obj) {
  java_ref_ = JavaObjectWeakGlobalRef(env, obj);
}

void AppWebMessagePortServiceImpl::CreateMessageChannel(
    JNIEnv* env,
    jobjectArray ports,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  int routing_id = web_contents->GetMainFrame()->GetRoutingID();
  scoped_refptr<AppWebMessagePortMessageFilter> filter =
      rfh->GetAppWebMessagePortMessageFilter(routing_id);
  ScopedJavaGlobalRef<jobjectArray>* j_ports =
      new ScopedJavaGlobalRef<jobjectArray>();
  j_ports->Reset(env, ports);

  int* portId1 = new int;
  int* portId2 = new int;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AppWebMessagePortServiceImpl::CreateMessageChannelOnIOThread,
                 base::Unretained(this), filter, portId1, portId2),
      base::Bind(&AppWebMessagePortServiceImpl::OnMessageChannelCreated,
                 base::Unretained(this), base::Owned(j_ports),
                 base::Owned(portId1), base::Owned(portId2)));
}

void AppWebMessagePortServiceImpl::OnConvertedWebToAppMessage(
    int message_port_id,
    const base::ListValue& message,
    const std::vector<int>& sent_message_port_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_ref_.get(env);
  if (jobj.is_null())
    return;

  base::string16 value;
  if (!message.GetString(0, &value)) {
    LOG(WARNING) << "Converting post message to a string failed for port "
                 << message_port_id;
    return;
  }

  if (message.GetSize() != 1) {
    NOTREACHED();
    return;
  }

  // Add the ports to AppWebMessagePortService.
  for (const auto& iter : sent_message_port_ids) {
    AddPort(iter, ports_[message_port_id]);
  }

  ScopedJavaLocalRef<jstring> jmsg = ConvertUTF16ToJavaString(env, value);
  ScopedJavaLocalRef<jintArray> jports =
      ToJavaIntArray(env, sent_message_port_ids);
  Java_AppWebMessagePortService_onReceivedMessage(env, jobj, message_port_id,
                                                  jmsg, jports);
}

void AppWebMessagePortServiceImpl::OnMessagePortMessageFilterClosing(
    AppWebMessagePortMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (MessagePorts::iterator iter = ports_.begin(); iter != ports_.end();
       iter++) {
    if (iter->second == filter) {
      ports_.erase(iter);
    }
  }
}

void AppWebMessagePortServiceImpl::PostAppToWebMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int sender_id,
    const JavaParamRef<jstring>& message,
    const JavaParamRef<jintArray>& sent_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::string16* j_message = new base::string16;
  ConvertJavaStringToUTF16(env, message, j_message);
  std::vector<int>* j_sent_ports = new std::vector<int>;
  if (sent_ports != nullptr)
    base::android::JavaIntArrayToIntVector(env, sent_ports, j_sent_ports);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AppWebMessagePortServiceImpl::PostAppToWebMessageOnIOThread,
                 base::Unretained(this), sender_id, base::Owned(j_message),
                 base::Owned(j_sent_ports)));
}

// The message port service cannot immediately close the port, because
// it is possible that messages are still queued in the renderer process
// waiting for a conversion. Instead, it sends a special message with
// a flag which indicates that this message port should be closed.
void AppWebMessagePortServiceImpl::ClosePort(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AppWebMessagePortServiceImpl::PostClosePortMessage,
                 base::Unretained(this), message_port_id));
}

void AppWebMessagePortServiceImpl::ReleaseMessages(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MessagePortService::ReleaseMessages,
                 base::Unretained(MessagePortService::GetInstance()),
                 message_port_id));
}

void AppWebMessagePortServiceImpl::RemoveSentPorts(
    const std::vector<int>& sent_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove the filters that are associated with the transferred ports
  for (const auto& iter : sent_ports)
    ports_.erase(iter);
}

void AppWebMessagePortServiceImpl::PostAppToWebMessageOnIOThread(
    int sender_id,
    base::string16* message,
    std::vector<int>* sent_ports) {
  RemoveSentPorts(*sent_ports);
  ports_[sender_id]->SendAppToWebMessage(sender_id, *message, *sent_ports);
}

void AppWebMessagePortServiceImpl::PostClosePortMessage(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ports_[message_port_id]->SendClosePortMessage(message_port_id);
}

void AppWebMessagePortServiceImpl::CleanupPort(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ports_.erase(message_port_id);
}

void AppWebMessagePortServiceImpl::CreateMessageChannelOnIOThread(
    scoped_refptr<AppWebMessagePortMessageFilter> filter,
    int* portId1,
    int* portId2) {
  *portId1 = 0;
  *portId2 = 0;
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->Create(MSG_ROUTING_NONE, filter.get(), portId1);
  msp->Create(MSG_ROUTING_NONE, filter.get(), portId2);
  // Update the routing number of the message ports to be equal to the message
  // port numbers.
  msp->UpdateMessagePort(*portId1, filter.get(), *portId1);
  msp->UpdateMessagePort(*portId2, filter.get(), *portId2);
  msp->Entangle(*portId1, *portId2);
  msp->Entangle(*portId2, *portId1);

  msp->HoldMessages(*portId1);
  msp->HoldMessages(*portId2);
  AddPort(*portId1, filter.get());
  AddPort(*portId2, filter.get());
}

void AppWebMessagePortServiceImpl::OnMessageChannelCreated(
    ScopedJavaGlobalRef<jobjectArray>* ports,
    int* port1,
    int* port2) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AppWebMessagePortService_onMessageChannelCreated(env, obj, *port1,
                                                        *port2, *ports);
}

// Adds a new port to the message port service.
void AppWebMessagePortServiceImpl::AddPort(
    int message_port_id,
    AppWebMessagePortMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }
  ports_[message_port_id] = filter;
}

bool RegisterAppWebMessagePortService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jlong InitAppWebMessagePortService(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  AppWebMessagePortServiceImpl* service =
      AppWebMessagePortServiceImpl::GetInstance();
  service->Init(env, obj);
  return reinterpret_cast<intptr_t>(service);
}

}  // namespace content
