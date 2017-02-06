// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TOOLBAR_TEST_TOOLBAR_MODEL_H_
#define COMPONENTS_TOOLBAR_TEST_TOOLBAR_MODEL_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/toolbar/toolbar_model.h"

namespace gfx {
enum class VectorIconId;
}

// A ToolbarModel that is backed by instance variables, which are initialized
// with some basic values that can be changed with the provided setters. This
// should be used only for testing.
class TestToolbarModel : public ToolbarModel {
 public:
  TestToolbarModel();
  ~TestToolbarModel() override;
  base::string16 GetText() const override;
  base::string16 GetFormattedURL(size_t* prefix_end) const override;
  GURL GetURL() const override;
  bool WouldPerformSearchTermReplacement(bool ignore_editing) const override;
  security_state::SecurityStateModel::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const override;
  int GetIcon() const override;
  gfx::VectorIconId GetVectorIcon() const override;
  base::string16 GetEVCertName() const override;
  bool ShouldDisplayURL() const override;

  void set_text(const base::string16& text) { text_ = text; }
  void set_url(const GURL& url) { url_ = url;}
  void set_perform_search_term_replacement(
      bool perform_search_term_replacement) {
    perform_search_term_replacement_ = perform_search_term_replacement;
  }
  void set_security_level(
      security_state::SecurityStateModel::SecurityLevel security_level) {
    security_level_ = security_level;
  }
  void set_icon(gfx::VectorIconId icon) { icon_ = icon; }
  void set_ev_cert_name(const base::string16& ev_cert_name) {
    ev_cert_name_ = ev_cert_name;
  }
  void set_should_display_url(bool should_display_url) {
    should_display_url_ = should_display_url;
  }

 private:
  base::string16 text_;
  GURL url_;
  bool perform_search_term_replacement_;
  security_state::SecurityStateModel::SecurityLevel security_level_;
  gfx::VectorIconId icon_;
  base::string16 ev_cert_name_;
  bool should_display_url_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarModel);
};

#endif  // COMPONENTS_TOOLBAR_TEST_TOOLBAR_MODEL_H_
