// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_DELEGATE_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_DELEGATE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "net/base/network_change_notifier.h"

namespace net {

// Delegate used to thread-safely notify NetworkChangeNotifierAndroid whenever a
// network connection change notification is signaled by the Java side (on the
// JNI thread).
// All the methods exposed below must be called exclusively on the JNI thread
// unless otherwise stated (e.g. AddObserver()/RemoveObserver()).
class NET_EXPORT_PRIVATE NetworkChangeNotifierDelegateAndroid {
 public:
  typedef NetworkChangeNotifier::ConnectionType ConnectionType;

  // Observer interface implemented by NetworkChangeNotifierAndroid which
  // subscribes to network change notifications fired by the delegate (and
  // initiated by the Java side).
  class Observer {
   public:
    virtual ~Observer() {}

    // Updates the current connection type.
    virtual void OnConnectionTypeChanged() = 0;
  };

  NetworkChangeNotifierDelegateAndroid();
  ~NetworkChangeNotifierDelegateAndroid();

  // Called from NetworkChangeNotifierAndroid.java on the JNI thread whenever
  // the connection type changes. This updates the current connection type seen
  // by this class and forwards the notification to the observers that
  // subscribed through AddObserver().
  void NotifyConnectionTypeChanged(JNIEnv* env,
                                   jobject obj,
                                   jint new_connection_type);
  jint GetConnectionType(JNIEnv* env, jobject obj) const;

  // These methods can be called on any thread. Note that the provided observer
  // will be notified on the thread AddObserver() is called on.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Can be called from any thread.
  ConnectionType GetCurrentConnectionType() const;

  // Initializes JNI bindings.
  static bool Register(JNIEnv* env);

 private:
  friend class BaseNetworkChangeNotifierAndroidTest;

  void SetCurrentConnectionType(ConnectionType connection_type);

  // Methods calling the Java side exposed for testing.
  void SetOnline();
  void SetOffline();

  base::ThreadChecker thread_checker_;
  scoped_refptr<ObserverListThreadSafe<Observer> > observers_;
  scoped_refptr<base::SingleThreadTaskRunner> jni_task_runner_;
  base::android::ScopedJavaGlobalRef<jobject> java_network_change_notifier_;
  mutable base::Lock connection_type_lock_;  // Protects the state below.
  ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierDelegateAndroid);
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_DELEGATE_H_
