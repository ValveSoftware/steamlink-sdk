// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_H_
#define BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace cc {
class Layer;
}  // namespace cc

namespace blimp {
namespace client {

class BlimpContentsObserver;
class BlimpContentsView;
class BlimpNavigationController;

// BlimpContents is the core class in blimp client which is responsible for
// rendering web pages in a rectangular area. The interaction with between the
// client and engine is encapsulated behind this interface through the use of
// the navigation controller. Each BlimpContents has exactly one
// BlimpNavigationController which can be used to load URLs, navigate
// backwards/forward etc. See blimp_navigation_controller.h for more details.
class BlimpContents : public base::SupportsUserData {
 public:
  // Retrives the navigation controller that controls all navigation related
  // to this BlimpContents.
  virtual BlimpNavigationController& GetNavigationController() = 0;

  // Enables adding and removing observers to this BlimpContents.
  virtual void AddObserver(BlimpContentsObserver* observer) = 0;
  virtual void RemoveObserver(BlimpContentsObserver* observer) = 0;

  // Returns the view that holds the contents of this tab.
  virtual BlimpContentsView* GetView() = 0;

  // Will cause this BlimpContents and the remote contents to show and start or
  // stop rendering content respectively.
  virtual void Show() = 0;
  virtual void Hide() = 0;

#if defined(OS_ANDROID)
  // Returns a Java object of the type BlimpContents for the given
  // BlimpContents.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
#endif

 protected:
  BlimpContents() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContents);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_H_
