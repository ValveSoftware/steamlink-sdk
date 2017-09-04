// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class GuestViewInternalCreateGuestFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.createGuest",
                             GUESTVIEWINTERNAL_CREATEGUEST);
  GuestViewInternalCreateGuestFunction();

 protected:
  ~GuestViewInternalCreateGuestFunction() override {}
  bool RunAsync() final;

 private:
  void CreateGuestCallback(content::WebContents* guest_web_contents);
  DISALLOW_COPY_AND_ASSIGN(GuestViewInternalCreateGuestFunction);
};

class GuestViewInternalDestroyGuestFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.destroyGuest",
                             GUESTVIEWINTERNAL_DESTROYGUEST);
  GuestViewInternalDestroyGuestFunction();

 protected:
  ~GuestViewInternalDestroyGuestFunction() override;
  bool RunAsync() final;

 private:
  void DestroyGuestCallback(content::WebContents* guest_web_contents);
  DISALLOW_COPY_AND_ASSIGN(GuestViewInternalDestroyGuestFunction);
};

class GuestViewInternalSetSizeFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.setSize",
                             GUESTVIEWINTERNAL_SETAUTOSIZE);

  GuestViewInternalSetSizeFunction();

 protected:
  ~GuestViewInternalSetSizeFunction() override;
  bool RunAsync() final;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestViewInternalSetSizeFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
