// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/content/common/distillability_service.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace dom_distiller {

// This is an IPC helper for determining whether a page should be distilled.
class DistillabilityDriver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DistillabilityDriver> {
 public:
  ~DistillabilityDriver() override;
  void CreateDistillabilityService(
      mojo::InterfaceRequest<mojom::DistillabilityService> request);

  void SetDelegate(const base::Callback<void(bool, bool)>& delegate);

  // content::WebContentsObserver implementation.
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void RenderFrameHostChanged(
      content::RenderFrameHost* old_host,
      content::RenderFrameHost* new_host) override;

 private:
  explicit DistillabilityDriver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<DistillabilityDriver>;
  friend class DistillabilityServiceImpl;

  void SetupMojoService(content::RenderFrameHost* frame_host);
  void OnDistillability(bool distillable, bool is_last);

  void SetNeedsMojoSetup();

  base::Callback<void(bool, bool)> m_delegate_;

  bool mojo_needs_setup_;

  base::WeakPtrFactory<DistillabilityDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DistillabilityDriver);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_
