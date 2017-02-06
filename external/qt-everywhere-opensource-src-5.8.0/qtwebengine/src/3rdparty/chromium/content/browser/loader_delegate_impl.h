// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_LOADER_DELEGATE_IMPL_H_

#include "content/browser/loader/loader_delegate.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT LoaderDelegateImpl : public LoaderDelegate {
 public:
  ~LoaderDelegateImpl() override;

  // LoaderDelegate implementation:
  void LoadStateChanged(int child_id,
                        int route_id,
                        const GURL& url,
                        const net::LoadStateWithParam& load_state,
                        uint64_t upload_position,
                        uint64_t upload_size) override;
  void DidGetResourceResponseStart(
      int render_process_id,
      int render_frame_host,
      std::unique_ptr<ResourceRequestDetails> details) override;
  void DidGetRedirectForResourceRequest(
      int render_process_id,
      int render_frame_host,
      std::unique_ptr<ResourceRedirectDetails> details) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DELEGATE_IMPL_H_
