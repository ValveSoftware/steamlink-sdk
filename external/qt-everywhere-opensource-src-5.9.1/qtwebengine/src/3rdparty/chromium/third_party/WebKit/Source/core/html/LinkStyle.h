// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkStyle_h
#define LinkStyle_h

#include "core/dom/Node.h"
#include "core/dom/StyleEngine.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/StyleSheetResource.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/html/LinkResource.h"

namespace blink {

class HTMLLinkElement;
//
// LinkStyle handles dynamically change-able link resources, which is
// typically @rel="stylesheet".
//
// It could be @rel="shortcut icon" or something else though. Each of
// types might better be handled by a separate class, but dynamically
// changing @rel makes it harder to move such a design so we are
// sticking current way so far.
//
class LinkStyle final : public LinkResource, ResourceOwner<StyleSheetResource> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkStyle);

 public:
  static LinkStyle* create(HTMLLinkElement* owner);

  explicit LinkStyle(HTMLLinkElement* owner);
  ~LinkStyle() override;

  LinkResourceType type() const override { return Style; }
  void process() override;
  void ownerRemoved() override;
  bool hasLoaded() const override { return m_loadedSheet; }
  DECLARE_VIRTUAL_TRACE();

  void startLoadingDynamicSheet();
  void notifyLoadedSheetAndAllCriticalSubresources(
      Node::LoadedSheetErrorStatus);
  bool sheetLoaded();

  void setDisabledState(bool);
  void setSheetTitle(
      const String&,
      StyleEngine::ActiveSheetsUpdate = StyleEngine::DontUpdateActiveSheets);

  bool styleSheetIsLoading() const;
  bool hasSheet() const { return m_sheet; }
  bool isDisabled() const { return m_disabledState == Disabled; }
  bool isEnabledViaScript() const {
    return m_disabledState == EnabledViaScript;
  }
  bool isUnset() const { return m_disabledState == Unset; }

  CSSStyleSheet* sheet() const { return m_sheet.get(); }

 private:
  // From StyleSheetResourceClient
  void setCSSStyleSheet(const String& href,
                        const KURL& baseURL,
                        const String& charset,
                        const CSSStyleSheetResource*) override;
  String debugName() const override { return "LinkStyle"; }
  enum LoadReturnValue { Loaded, NotNeeded, Bail };
  LoadReturnValue loadStylesheetIfNeeded(const LinkRequestBuilder&,
                                         const String& type);

  enum DisabledState { Unset, EnabledViaScript, Disabled };

  enum PendingSheetType { None, NonBlocking, Blocking };

  void clearSheet();
  void addPendingSheet(PendingSheetType);
  void removePendingSheet();
  Document& document();

  void setCrossOriginStylesheetStatus(CSSStyleSheet*);
  void setFetchFollowingCORS() {
    DCHECK(!m_fetchFollowingCORS);
    m_fetchFollowingCORS = true;
  }
  void clearFetchFollowingCORS() { m_fetchFollowingCORS = false; }

  Member<CSSStyleSheet> m_sheet;
  DisabledState m_disabledState;
  PendingSheetType m_pendingSheetType;
  StyleEngineContext m_styleEngineContext;
  bool m_loading;
  bool m_firedLoad;
  bool m_loadedSheet;
  bool m_fetchFollowingCORS;
};

}  // namespace blink

#endif
