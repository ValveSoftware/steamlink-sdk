// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

using infobars::InfoBar;
using infobars::InfoBarManager;

namespace extensions {

namespace {

IncognitoConnectability::ScopedAlertTracker::Mode g_alert_mode =
    IncognitoConnectability::ScopedAlertTracker::INTERACTIVE;
int g_alert_count = 0;

class IncognitoConnectabilityInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  typedef base::Callback<void(
      IncognitoConnectability::ScopedAlertTracker::Mode)> InfoBarCallback;

  // Creates a confirmation infobar and delegate and adds the infobar to
  // |infobar_service|.
  static InfoBar* Create(InfoBarManager* infobar_manager,
                         const base::string16& message,
                         const InfoBarCallback& callback);

  // Marks the infobar as answered so that the callback is not executed when the
  // delegate is destroyed.
  void set_answered() { answered_ = true; }

 private:
  IncognitoConnectabilityInfoBarDelegate(const base::string16& message,
                                         const InfoBarCallback& callback);
  ~IncognitoConnectabilityInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  base::string16 message_;
  bool answered_;
  InfoBarCallback callback_;
};

// static
InfoBar* IncognitoConnectabilityInfoBarDelegate::Create(
    InfoBarManager* infobar_manager,
    const base::string16& message,
    const IncognitoConnectabilityInfoBarDelegate::InfoBarCallback& callback) {
  return infobar_manager->AddInfoBar(infobar_manager->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new IncognitoConnectabilityInfoBarDelegate(message, callback))));
}

IncognitoConnectabilityInfoBarDelegate::IncognitoConnectabilityInfoBarDelegate(
    const base::string16& message,
    const InfoBarCallback& callback)
    : message_(message), answered_(false), callback_(callback) {
}

IncognitoConnectabilityInfoBarDelegate::
    ~IncognitoConnectabilityInfoBarDelegate() {
  if (!answered_) {
    // The infobar has closed without the user expressing an explicit
    // preference. The current request should be denied but further requests
    // should show an interactive prompt.
    callback_.Run(IncognitoConnectability::ScopedAlertTracker::INTERACTIVE);
  }
}

infobars::InfoBarDelegate::Type
IncognitoConnectabilityInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
IncognitoConnectabilityInfoBarDelegate::GetIdentifier() const {
  return INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE;
}

base::string16 IncognitoConnectabilityInfoBarDelegate::GetMessageText() const {
  return message_;
}

base::string16 IncognitoConnectabilityInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_PERMISSION_ALLOW : IDS_PERMISSION_DENY);
}

bool IncognitoConnectabilityInfoBarDelegate::Accept() {
  callback_.Run(IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);
  answered_ = true;
  return true;
}

bool IncognitoConnectabilityInfoBarDelegate::Cancel() {
  callback_.Run(IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY);
  answered_ = true;
  return true;
}

}  // namespace

IncognitoConnectability::ScopedAlertTracker::ScopedAlertTracker(Mode mode)
    : last_checked_invocation_count_(g_alert_count) {
  DCHECK_EQ(INTERACTIVE, g_alert_mode);
  DCHECK_NE(INTERACTIVE, mode);
  g_alert_mode = mode;
}

IncognitoConnectability::ScopedAlertTracker::~ScopedAlertTracker() {
  DCHECK_NE(INTERACTIVE, g_alert_mode);
  g_alert_mode = INTERACTIVE;
}

int IncognitoConnectability::ScopedAlertTracker::GetAndResetAlertCount() {
  int result = g_alert_count - last_checked_invocation_count_;
  last_checked_invocation_count_ = g_alert_count;
  return result;
}

IncognitoConnectability::IncognitoConnectability(
    content::BrowserContext* context)
    : weak_factory_(this) {
  CHECK(context->IsOffTheRecord());
}

IncognitoConnectability::~IncognitoConnectability() {
}

// static
IncognitoConnectability* IncognitoConnectability::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<IncognitoConnectability>::Get(context);
}

void IncognitoConnectability::Query(
    const Extension* extension,
    content::WebContents* web_contents,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  GURL origin = url.GetOrigin();
  if (origin.is_empty()) {
    callback.Run(false);
    return;
  }

  if (IsInMap(extension, origin, allowed_origins_)) {
    callback.Run(true);
    return;
  }

  if (IsInMap(extension, origin, disallowed_origins_)) {
    callback.Run(false);
    return;
  }

  PendingOrigin& pending_origin =
      pending_origins_[make_pair(extension->id(), origin)];
  InfoBarManager* infobar_manager =
      InfoBarService::FromWebContents(web_contents);
  TabContext& tab_context = pending_origin[infobar_manager];
  tab_context.callbacks.push_back(callback);
  if (tab_context.infobar) {
    // This tab is already displaying an infobar for this extension and origin.
    return;
  }

  // We need to ask the user.
  ++g_alert_count;

  switch (g_alert_mode) {
    // Production code should always be using INTERACTIVE.
    case ScopedAlertTracker::INTERACTIVE: {
      int template_id =
          extension->is_app()
              ? IDS_EXTENSION_PROMPT_APP_CONNECT_FROM_INCOGNITO
              : IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO;
      tab_context.infobar = IncognitoConnectabilityInfoBarDelegate::Create(
          infobar_manager, l10n_util::GetStringFUTF16(
                               template_id, base::UTF8ToUTF16(origin.spec()),
                               base::UTF8ToUTF16(extension->name())),
          base::Bind(&IncognitoConnectability::OnInteractiveResponse,
                     weak_factory_.GetWeakPtr(), extension->id(), origin,
                     infobar_manager));
      break;
    }

    // Testing code can override to always allow or deny.
    case ScopedAlertTracker::ALWAYS_ALLOW:
    case ScopedAlertTracker::ALWAYS_DENY:
      OnInteractiveResponse(extension->id(), origin, infobar_manager,
                            g_alert_mode);
      break;
  }
}

IncognitoConnectability::TabContext::TabContext() : infobar(nullptr) {
}

IncognitoConnectability::TabContext::TabContext(const TabContext& other) =
    default;

IncognitoConnectability::TabContext::~TabContext() {
}

void IncognitoConnectability::OnInteractiveResponse(
    const std::string& extension_id,
    const GURL& origin,
    InfoBarManager* infobar_manager,
    ScopedAlertTracker::Mode response) {
  switch (response) {
    case ScopedAlertTracker::ALWAYS_ALLOW:
      allowed_origins_[extension_id].insert(origin);
      break;
    case ScopedAlertTracker::ALWAYS_DENY:
      disallowed_origins_[extension_id].insert(origin);
      break;
    default:
      // Otherwise the user has not expressed an explicit preference and so
      // nothing should be permanently recorded.
      break;
  }

  DCHECK(base::ContainsKey(pending_origins_, make_pair(extension_id, origin)));
  PendingOrigin& pending_origin =
      pending_origins_[make_pair(extension_id, origin)];
  DCHECK(base::ContainsKey(pending_origin, infobar_manager));

  std::vector<base::Callback<void(bool)>> callbacks;
  if (response == ScopedAlertTracker::INTERACTIVE) {
    // No definitive answer for this extension and origin. Execute only the
    // callbacks associated with this tab.
    TabContext& tab_context = pending_origin[infobar_manager];
    callbacks.swap(tab_context.callbacks);
    pending_origin.erase(infobar_manager);
  } else {
    // We have a definitive answer for this extension and origin. Close all
    // other infobars and answer all the callbacks.
    for (const auto& map_entry : pending_origin) {
      InfoBarManager* other_infobar_manager = map_entry.first;
      const TabContext& other_tab_context = map_entry.second;
      if (other_infobar_manager != infobar_manager) {
        // Disarm the delegate so that it doesn't think the infobar has been
        // dismissed.
        IncognitoConnectabilityInfoBarDelegate* delegate =
            static_cast<IncognitoConnectabilityInfoBarDelegate*>(
                other_tab_context.infobar->delegate());
        delegate->set_answered();
        other_infobar_manager->RemoveInfoBar(other_tab_context.infobar);
      }
      callbacks.insert(callbacks.end(), other_tab_context.callbacks.begin(),
                       other_tab_context.callbacks.end());
    }
    pending_origins_.erase(make_pair(extension_id, origin));
  }

  DCHECK(!callbacks.empty());
  for (const auto& callback : callbacks) {
    callback.Run(response == ScopedAlertTracker::ALWAYS_ALLOW);
  }
}

bool IncognitoConnectability::IsInMap(const Extension* extension,
                                      const GURL& origin,
                                      const ExtensionToOriginsMap& map) {
  DCHECK_EQ(origin, origin.GetOrigin());
  ExtensionToOriginsMap::const_iterator it = map.find(extension->id());
  return it != map.end() && it->second.count(origin) > 0;
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<IncognitoConnectability> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<IncognitoConnectability>*
IncognitoConnectability::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
