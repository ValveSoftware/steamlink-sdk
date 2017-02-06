// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_BLIMP_CONTENTS_IMPL_H_
#define BLIMP_CLIENT_CORE_BLIMP_CONTENTS_IMPL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "blimp/client/core/blimp_navigation_controller_delegate.h"
#include "blimp/client/core/blimp_navigation_controller_impl.h"
#include "blimp/client/core/public/blimp_contents.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {

#if defined(OS_ANDROID)
class BlimpContentsImplAndroid;
#endif  // defined(OS_ANDROID)

class BlimpContentsObserver;
class BlimpNavigationController;

// BlimpContentsImpl is the implementation of the core class in
// //blimp/client/core, the BlimpContents.
class BlimpContentsImpl : public BlimpContents,
                          public BlimpNavigationControllerDelegate,
                          base::SupportsUserData {
 public:
  BlimpContentsImpl();
  ~BlimpContentsImpl() override;

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaBlimpContentsImpl();
  BlimpContentsImplAndroid* GetBlimpContentsImplAndroid();
#endif  // defined(OS_ANDROID)

  // BlimpContents implementation.
  BlimpNavigationControllerImpl& GetNavigationController() override;
  void AddObserver(BlimpContentsObserver* observer) override;
  void RemoveObserver(BlimpContentsObserver* observer) override;

  // BlimpNavigationControllerDelegate implementation.
  void NotifyURLLoaded(const GURL& url) override;

 private:
  // Handles the back/forward list and loading URLs.
  BlimpNavigationControllerImpl navigation_controller_;

  // A list of all the observers of this BlimpContentsImpl.
  base::ObserverList<BlimpContentsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_BLIMP_CONTENTS_IMPL_H_
