// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_common.h"

#if defined(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#endif  // defined(ENABLE_EXTENSIONS)

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_PRINT_PREVIEW)

namespace printing {
namespace {
#if defined(ENABLE_EXTENSIONS)
// Stores |guest_contents| in |result| and returns true if |guest_contents| is a
// full page MimeHandlerViewGuest plugin. Otherwise, returns false.
bool StoreFullPagePlugin(content::WebContents** result,
                         content::WebContents* guest_contents) {
  auto guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(guest_contents);
  if (guest_view && guest_view->is_full_page_plugin()) {
    *result = guest_contents;
    return true;
  }
  return false;
}
#endif  // defined(ENABLE_EXTENSIONS)

// If we have a single full-page embedded mime handler view guest, print the
// guest's WebContents instead.
content::WebContents* GetWebContentsToUse(content::WebContents* contents) {
#if defined(ENABLE_EXTENSIONS)
  guest_view::GuestViewManager* guest_view_manager =
      guest_view::GuestViewManager::FromBrowserContext(
          contents->GetBrowserContext());
  if (guest_view_manager) {
    guest_view_manager->ForEachGuest(
        contents,
        base::Bind(&StoreFullPagePlugin, &contents));
  }
#endif  // defined(ENABLE_EXTENSIONS)
  return contents;
}

}  // namespace

void StartPrint(content::WebContents* contents,
                bool print_preview_disabled,
                bool selection_only) {
#if defined(ENABLE_PRINT_PREVIEW)
  using PrintViewManagerImpl = PrintViewManager;
#else
  using PrintViewManagerImpl = PrintViewManagerBasic;
#endif  // defined(ENABLE_PRINT_PREVIEW)

  auto print_view_manager =
      PrintViewManagerImpl::FromWebContents(GetWebContentsToUse(contents));
  if (!print_view_manager)
    return;
#if defined(ENABLE_PRINT_PREVIEW)
  if (!print_preview_disabled) {
    print_view_manager->PrintPreviewNow(selection_only);
    return;
  }
#endif  // ENABLE_PRINT_PREVIEW

#if defined(ENABLE_BASIC_PRINTING)
  print_view_manager->PrintNow();
#endif  // ENABLE_BASIC_PRINTING
}

#if defined(ENABLE_BASIC_PRINTING)
void StartBasicPrint(content::WebContents* contents) {
#if defined(ENABLE_PRINT_PREVIEW)
  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(GetWebContentsToUse(contents));
  if (!print_view_manager)
    return;
  print_view_manager->BasicPrint();
#endif  // ENABLE_PRINT_PREVIEW
}
#endif  // ENABLE_BASIC_PRINTING

}  // namespace printing
