// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TOOLBAR_TOOLBAR_MODEL_IMPL_H_
#define COMPONENTS_TOOLBAR_TOOLBAR_MODEL_IMPL_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/toolbar/toolbar_model.h"
#include "url/gurl.h"

class ToolbarModelDelegate;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModelImpl : public ToolbarModel {
 public:
  ToolbarModelImpl(ToolbarModelDelegate* delegate,
                   size_t max_url_display_chars);
  ~ToolbarModelImpl() override;

 private:
  // ToolbarModel:
  base::string16 GetFormattedURL(size_t* prefix_end) const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const override;
  gfx::VectorIconId GetVectorIcon() const override;
  base::string16 GetSecureVerboseText() const override;
  base::string16 GetEVCertName() const override;
  bool ShouldDisplayURL() const override;

  ToolbarModelDelegate* delegate_;
  const size_t max_url_display_chars_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelImpl);
};

#endif  // COMPONENTS_TOOLBAR_TOOLBAR_MODEL_IMPL_H_
