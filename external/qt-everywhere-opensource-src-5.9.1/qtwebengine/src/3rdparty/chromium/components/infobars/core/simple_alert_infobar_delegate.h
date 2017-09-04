// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/gfx/vector_icons_public.h"

namespace infobars {
class InfoBarManager;
}

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a simple alert infobar and delegate and adds the infobar to
  // |infobar_manager|. If |vector_icon_id| is not VECTOR_ICON_NONE, it will be
  // shown; otherwise, |icon_id| (if present) will be used as the icon.
  // |infobar_identifier| names what class triggered the infobar for metrics.
  static void Create(
      infobars::InfoBarManager* infobar_manager,
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      int icon_id,
      gfx::VectorIconId vector_icon_id,
      const base::string16& message,
      bool auto_expire);

 private:
  SimpleAlertInfoBarDelegate(
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      int icon_id,
      gfx::VectorIconId vector_icon_id,
      const base::string16& message,
      bool auto_expire);
  ~SimpleAlertInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  gfx::VectorIconId GetVectorIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;

  infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier_;
  const int icon_id_;
  gfx::VectorIconId vector_icon_id_;
  base::string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
