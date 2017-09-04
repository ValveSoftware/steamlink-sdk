// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CLIPBOARD_CLIPBOARD_API_H_
#define EXTENSIONS_BROWSER_API_CLIPBOARD_CLIPBOARD_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace extensions {

class ClipboardAPI : public BrowserContextKeyedAPI,
                     public ui::ClipboardObserver {
 public:
  explicit ClipboardAPI(content::BrowserContext* context);
  ~ClipboardAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ClipboardAPI>* GetFactoryInstance();

  // ui::ClipboardObserver implementation.
  void OnClipboardDataChanged() override;

 private:
  friend class BrowserContextKeyedAPIFactory<ClipboardAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ClipboardAPI"; }

  content::BrowserContext* const browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CLIPBOARD_CLIPBOARD_API_H_
