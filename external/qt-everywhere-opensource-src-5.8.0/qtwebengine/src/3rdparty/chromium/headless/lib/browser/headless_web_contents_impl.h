// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_

#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"

#include <memory>
#include <unordered_map>

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
class DevToolsAgentHost;
class WebContents;
}

namespace gfx {
class Size;
}

namespace headless {
class HeadlessDevToolsHostImpl;
class HeadlessBrowserImpl;
class WebContentsObserverAdapter;

class HeadlessWebContentsImpl : public HeadlessWebContents,
                                public HeadlessDevToolsTarget {
 public:
  ~HeadlessWebContentsImpl() override;

  static std::unique_ptr<HeadlessWebContentsImpl> Create(
      HeadlessWebContents::Builder* builder,
      aura::Window* parent_window,
      HeadlessBrowserImpl* browser);

  // Takes ownership of |web_contents|.
  static std::unique_ptr<HeadlessWebContentsImpl> CreateFromWebContents(
      content::WebContents* web_contents,
      HeadlessBrowserImpl* browser);

  // HeadlessWebContents implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  HeadlessDevToolsTarget* GetDevToolsTarget() override;

  // HeadlessDevToolsTarget implementation:
  void AttachClient(HeadlessDevToolsClient* client) override;
  void DetachClient(HeadlessDevToolsClient* client) override;

  content::WebContents* web_contents() const;
  bool OpenURL(const GURL& url);

  void Close() override;

 private:
  // Takes ownership of |web_contents|.
  HeadlessWebContentsImpl(content::WebContents* web_contents,
                          HeadlessBrowserImpl* browser);

  void InitializeScreen(aura::Window* parent_window,
                        const gfx::Size& initial_size);

  class Delegate;
  std::unique_ptr<Delegate> web_contents_delegate_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;

  HeadlessBrowserImpl* browser_;  // Not owned.

  using ObserverMap =
      std::unordered_map<HeadlessWebContents::Observer*,
                         std::unique_ptr<WebContentsObserverAdapter>>;
  ObserverMap observer_map_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessWebContentsImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
