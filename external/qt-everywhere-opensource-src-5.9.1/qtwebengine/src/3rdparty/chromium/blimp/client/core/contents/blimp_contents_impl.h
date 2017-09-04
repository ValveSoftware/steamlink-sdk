// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_IMPL_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_IMPL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "blimp/client/core/contents/blimp_contents_view_impl.h"
#include "blimp/client/core/contents/blimp_navigation_controller_delegate.h"
#include "blimp/client/core/contents/blimp_navigation_controller_impl.h"
#include "blimp/client/core/render_widget/blimp_document_manager.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {

#if defined(OS_ANDROID)
class BlimpContentsImplAndroid;
#endif  // defined(OS_ANDROID)

class BlimpCompositorDependencies;
class BlimpContentsObserver;
class BlimpNavigationController;
class ImeFeature;
class NavigationFeature;
class RenderWidgetFeature;
class TabControlFeature;

class BlimpContentsImpl : public BlimpContents,
                          public BlimpNavigationControllerDelegate {
 public:
  // Ownership of the features remains with the caller.
  // |window| must be the platform specific window that this will be shown in.
  explicit BlimpContentsImpl(int id,
                             gfx::NativeWindow window,
                             BlimpCompositorDependencies* compositor_deps,
                             ImeFeature* ime_feature,
                             NavigationFeature* navigation_feature,
                             RenderWidgetFeature* render_widget_feature,
                             TabControlFeature* tab_control_feature);
  ~BlimpContentsImpl() override;

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  BlimpContentsImplAndroid* GetBlimpContentsImplAndroid();
#endif  // defined(OS_ANDROID)

  // BlimpContents implementation.
  BlimpNavigationControllerImpl& GetNavigationController() override;
  void AddObserver(BlimpContentsObserver* observer) override;
  void RemoveObserver(BlimpContentsObserver* observer) override;
  BlimpContentsViewImpl* GetView() override;
  void Show() override;
  void Hide() override;

  // Returns the platform specific window that this BlimpContents is showed in.
  gfx::NativeWindow GetNativeWindow();

  // Check if some observer is in the observer list.
  bool HasObserver(BlimpContentsObserver* observer);

  // BlimpNavigationControllerDelegate implementation.
  void OnNavigationStateChanged() override;
  void OnLoadingStateChanged(bool loading) override;
  void OnPageLoadingStateChanged(bool loading) override;

  // Pushes the size and scale information to the engine, which will affect the
  // web content display area for all tabs.
  void SetSizeAndScale(const gfx::Size& size, float device_pixel_ratio);

  int id() { return id_; }

  // TODO(nyquist): Remove this once the Android BlimpView uses a delegate.
  BlimpDocumentManager* document_manager() { return &document_manager_; }

 private:
  // Handles the back/forward list and loading URLs.
  BlimpNavigationControllerImpl navigation_controller_;

  // Holds onto all active BlimpDocument and BlimpCompositor instances for
  // this BlimpContents.
  // This properly exposes the right rendered page content for the correct
  // BlimpCompositor based on the engine state.
  BlimpDocumentManager document_manager_;

  // A list of all the observers of this BlimpContentsImpl.
  base::ObserverList<BlimpContentsObserver> observers_;

  // The id is assigned during contents creation. It is used by
  // BlimpContentsManager to control the life time of the its observer.
  int id_;

  // Handles the text input for web forms.
  ImeFeature* ime_feature_;

  // The platform specific window that this BlimpContents is showed in.
  gfx::NativeWindow window_;

  // The tab control feature through which the BlimpContentsImpl is able to
  // set size and scale.
  // TODO(mlliu): in the long term, we want to put size and scale in a different
  // feature instead of tab control feature. crbug.com/639154.
  TabControlFeature* tab_control_feature_ = nullptr;

  // The BlimpContentsView abstracts the platform specific view system details
  // from the BlimpContents.
  std::unique_ptr<BlimpContentsViewImpl> blimp_contents_view_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_IMPL_H_
