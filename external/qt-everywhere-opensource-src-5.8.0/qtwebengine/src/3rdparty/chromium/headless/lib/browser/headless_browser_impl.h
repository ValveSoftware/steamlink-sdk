// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_

#include "headless/public/headless_browser.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "headless/lib/browser/headless_web_contents_impl.h"

namespace aura {
class WindowTreeHost;

namespace client {
class WindowTreeClient;
}
}

namespace headless {

class HeadlessBrowserContext;
class HeadlessBrowserMainParts;

class HeadlessBrowserImpl : public HeadlessBrowser {
 public:
  HeadlessBrowserImpl(
      const base::Callback<void(HeadlessBrowser*)>& on_start_callback,
      HeadlessBrowser::Options options);
  ~HeadlessBrowserImpl() override;

  // HeadlessBrowser implementation:
  HeadlessWebContents::Builder CreateWebContentsBuilder() override;
  HeadlessBrowserContext::Builder CreateBrowserContextBuilder() override;
  HeadlessWebContents* CreateWebContents(const GURL& initial_url,
                                         const gfx::Size& size) override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserFileThread()
      const override;

  void Shutdown() override;

  std::vector<HeadlessWebContents*> GetAllWebContents() override;

  void set_browser_main_parts(HeadlessBrowserMainParts* browser_main_parts);
  HeadlessBrowserMainParts* browser_main_parts() const;

  void RunOnStartCallback();

  HeadlessBrowser::Options* options() { return &options_; }

  HeadlessWebContents* CreateWebContents(HeadlessWebContents::Builder* builder);
  HeadlessWebContentsImpl* RegisterWebContents(
      std::unique_ptr<HeadlessWebContentsImpl> web_contents);

  // Close given |web_contents| and delete it.
  void DestroyWebContents(HeadlessWebContentsImpl* web_contents);

  // Customize the options used by this headless browser instance. Note that
  // options which take effect before the message loop has been started (e.g.,
  // custom message pumps) cannot be set via this method.
  void SetOptionsForTesting(HeadlessBrowser::Options options);

 protected:
  base::Callback<void(HeadlessBrowser*)> on_start_callback_;
  HeadlessBrowser::Options options_;
  HeadlessBrowserMainParts* browser_main_parts_;  // Not owned.
  std::unique_ptr<aura::WindowTreeHost> window_tree_host_;
  std::unique_ptr<aura::client::WindowTreeClient> window_tree_client_;

  std::unordered_map<HeadlessWebContents*, std::unique_ptr<HeadlessWebContents>>
      web_contents_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
