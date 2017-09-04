// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/LinkStyle.h"

#include "core/css/StyleSheetContents.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/HTMLLinkElement.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/ContentType.h"
#include "platform/Histogram.h"
#include "platform/MIMETypeRegistry.h"

namespace blink {

using namespace HTMLNames;

static bool styleSheetTypeIsSupported(const String& type) {
  String trimmedType = ContentType(type).type();
  return trimmedType.isEmpty() ||
         MIMETypeRegistry::isSupportedStyleSheetMIMEType(trimmedType);
}

LinkStyle* LinkStyle::create(HTMLLinkElement* owner) {
  return new LinkStyle(owner);
}

LinkStyle::LinkStyle(HTMLLinkElement* owner)
    : LinkResource(owner),
      m_disabledState(Unset),
      m_pendingSheetType(None),
      m_loading(false),
      m_firedLoad(false),
      m_loadedSheet(false),
      m_fetchFollowingCORS(false) {}

LinkStyle::~LinkStyle() {}

Document& LinkStyle::document() {
  return m_owner->document();
}

enum StyleSheetCacheStatus {
  StyleSheetNewEntry,
  StyleSheetInDiskCache,
  StyleSheetInMemoryCache,
  StyleSheetCacheStatusCount,
};

void LinkStyle::setCSSStyleSheet(
    const String& href,
    const KURL& baseURL,
    const String& charset,
    const CSSStyleSheetResource* cachedStyleSheet) {
  if (!m_owner->isConnected()) {
    // While the stylesheet is asynchronously loading, the owner can be
    // disconnected from a document.
    // In that case, cancel any processing on the loaded content.
    m_loading = false;
    removePendingSheet();
    if (m_sheet)
      clearSheet();
    return;
  }

  // See the comment in PendingScript.cpp about why this check is necessary
  // here, instead of in the resource fetcher. https://crbug.com/500701.
  if (!cachedStyleSheet->errorOccurred() &&
      !m_owner->fastGetAttribute(HTMLNames::integrityAttr).isEmpty() &&
      !cachedStyleSheet->integrityMetadata().isEmpty()) {
    ResourceIntegrityDisposition disposition =
        cachedStyleSheet->integrityDisposition();

    if (disposition == ResourceIntegrityDisposition::NotChecked &&
        !cachedStyleSheet->loadFailedOrCanceled()) {
      bool checkResult;

      // cachedStyleSheet->resourceBuffer() can be nullptr on load success.
      // If response size == 0.
      const char* data = nullptr;
      size_t size = 0;
      if (cachedStyleSheet->resourceBuffer()) {
        data = cachedStyleSheet->resourceBuffer()->data();
        size = cachedStyleSheet->resourceBuffer()->size();
      }
      checkResult = SubresourceIntegrity::CheckSubresourceIntegrity(
          *m_owner, data, size, KURL(baseURL, href), *cachedStyleSheet);
      disposition = checkResult ? ResourceIntegrityDisposition::Passed
                                : ResourceIntegrityDisposition::Failed;

      // TODO(kouhei): Remove this const_cast crbug.com/653502
      const_cast<CSSStyleSheetResource*>(cachedStyleSheet)
          ->setIntegrityDisposition(disposition);
    }

    if (disposition == ResourceIntegrityDisposition::Failed) {
      m_loading = false;
      removePendingSheet();
      notifyLoadedSheetAndAllCriticalSubresources(
          Node::ErrorOccurredLoadingSubresource);
      return;
    }
  }

  CSSParserContext parserContext(m_owner->document(), nullptr, baseURL,
                                 charset);

  DEFINE_STATIC_LOCAL(EnumerationHistogram, restoredCachedStyleSheetHistogram,
                      ("Blink.RestoredCachedStyleSheet", 2));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, restoredCachedStyleSheet2Histogram,
      ("Blink.RestoredCachedStyleSheet2", StyleSheetCacheStatusCount));

  if (StyleSheetContents* restoredSheet =
          const_cast<CSSStyleSheetResource*>(cachedStyleSheet)
              ->restoreParsedStyleSheet(parserContext)) {
    DCHECK(restoredSheet->isCacheableForResource());
    DCHECK(!restoredSheet->isLoading());

    if (m_sheet)
      clearSheet();
    m_sheet = CSSStyleSheet::create(restoredSheet, *m_owner);
    m_sheet->setMediaQueries(MediaQuerySet::create(m_owner->media()));
    if (m_owner->isInDocumentTree())
      setSheetTitle(m_owner->title());
    setCrossOriginStylesheetStatus(m_sheet.get());

    m_loading = false;
    restoredSheet->checkLoaded();

    restoredCachedStyleSheetHistogram.count(true);
    restoredCachedStyleSheet2Histogram.count(StyleSheetInMemoryCache);
    return;
  }
  restoredCachedStyleSheetHistogram.count(false);
  StyleSheetCacheStatus cacheStatus = cachedStyleSheet->response().wasCached()
                                          ? StyleSheetInDiskCache
                                          : StyleSheetNewEntry;
  restoredCachedStyleSheet2Histogram.count(cacheStatus);

  StyleSheetContents* styleSheet =
      StyleSheetContents::create(href, parserContext);

  if (m_sheet)
    clearSheet();

  m_sheet = CSSStyleSheet::create(styleSheet, *m_owner);
  m_sheet->setMediaQueries(MediaQuerySet::create(m_owner->media()));
  if (m_owner->isInDocumentTree())
    setSheetTitle(m_owner->title());
  setCrossOriginStylesheetStatus(m_sheet.get());

  styleSheet->parseAuthorStyleSheet(cachedStyleSheet,
                                    m_owner->document().getSecurityOrigin());

  m_loading = false;
  styleSheet->notifyLoadedSheet(cachedStyleSheet);
  styleSheet->checkLoaded();

  if (styleSheet->isCacheableForResource()) {
    const_cast<CSSStyleSheetResource*>(cachedStyleSheet)
        ->saveParsedStyleSheet(styleSheet);
  }
  clearResource();
}

bool LinkStyle::sheetLoaded() {
  if (!styleSheetIsLoading()) {
    removePendingSheet();
    return true;
  }
  return false;
}

void LinkStyle::notifyLoadedSheetAndAllCriticalSubresources(
    Node::LoadedSheetErrorStatus errorStatus) {
  if (m_firedLoad)
    return;
  m_loadedSheet = (errorStatus == Node::NoErrorLoadingSubresource);
  if (m_owner)
    m_owner->scheduleEvent();
  m_firedLoad = true;
}

void LinkStyle::startLoadingDynamicSheet() {
  DCHECK_LT(m_pendingSheetType, Blocking);
  addPendingSheet(Blocking);
}

void LinkStyle::clearSheet() {
  DCHECK(m_sheet);
  DCHECK_EQ(m_sheet->ownerNode(), m_owner);
  m_sheet.release()->clearOwnerNode();
}

bool LinkStyle::styleSheetIsLoading() const {
  if (m_loading)
    return true;
  if (!m_sheet)
    return false;
  return m_sheet->contents()->isLoading();
}

void LinkStyle::addPendingSheet(PendingSheetType type) {
  if (type <= m_pendingSheetType)
    return;
  m_pendingSheetType = type;

  if (m_pendingSheetType == NonBlocking)
    return;
  m_owner->document().styleEngine().addPendingSheet(m_styleEngineContext);
}

void LinkStyle::removePendingSheet() {
  DCHECK(m_owner);
  PendingSheetType type = m_pendingSheetType;
  m_pendingSheetType = None;

  if (type == None)
    return;
  if (type == NonBlocking) {
    // Tell StyleEngine to re-compute styleSheets of this m_owner's treescope.
    m_owner->document().styleEngine().modifiedStyleSheetCandidateNode(*m_owner);
    return;
  }

  m_owner->document().styleEngine().removePendingSheet(*m_owner,
                                                       m_styleEngineContext);
}

void LinkStyle::setDisabledState(bool disabled) {
  LinkStyle::DisabledState oldDisabledState = m_disabledState;
  m_disabledState = disabled ? Disabled : EnabledViaScript;
  if (oldDisabledState != m_disabledState) {
    // If we change the disabled state while the sheet is still loading, then we
    // have to perform three checks:
    if (styleSheetIsLoading()) {
      // Check #1: The sheet becomes disabled while loading.
      if (m_disabledState == Disabled)
        removePendingSheet();

      // Check #2: An alternate sheet becomes enabled while it is still loading.
      if (m_owner->relAttribute().isAlternate() &&
          m_disabledState == EnabledViaScript)
        addPendingSheet(Blocking);

      // Check #3: A main sheet becomes enabled while it was still loading and
      // after it was disabled via script. It takes really terrible code to make
      // this happen (a double toggle for no reason essentially). This happens
      // on virtualplastic.net, which manages to do about 12 enable/disables on
      // only 3 sheets. :)
      if (!m_owner->relAttribute().isAlternate() &&
          m_disabledState == EnabledViaScript && oldDisabledState == Disabled)
        addPendingSheet(Blocking);

      // If the sheet is already loading just bail.
      return;
    }

    if (m_sheet) {
      m_sheet->setDisabled(disabled);
      return;
    }

    if (m_disabledState == EnabledViaScript && m_owner->shouldProcessStyle())
      process();
  }
}

void LinkStyle::setCrossOriginStylesheetStatus(CSSStyleSheet* sheet) {
  if (m_fetchFollowingCORS && resource() && !resource()->errorOccurred()) {
    // Record the security origin the CORS access check succeeded at, if cross
    // origin.  Only origins that are script accessible to it may access the
    // stylesheet's rules.
    sheet->setAllowRuleAccessFromOrigin(
        m_owner->document().getSecurityOrigin());
  }
  m_fetchFollowingCORS = false;
}

// TODO(yoav): move that logic to LinkLoader
LinkStyle::LoadReturnValue LinkStyle::loadStylesheetIfNeeded(
    const LinkRequestBuilder& builder,
    const String& type) {
  if (m_disabledState == Disabled || !m_owner->relAttribute().isStyleSheet() ||
      !styleSheetTypeIsSupported(type) || !shouldLoadResource() ||
      !builder.url().isValid())
    return NotNeeded;

  if (resource()) {
    removePendingSheet();
    clearResource();
    clearFetchFollowingCORS();
  }

  if (!m_owner->shouldLoadLink())
    return Bail;

  m_loading = true;

  String title = m_owner->title();
  if (!title.isEmpty() && !m_owner->isAlternate() &&
      m_disabledState != EnabledViaScript && m_owner->isInDocumentTree()) {
    document().styleEngine().setPreferredStylesheetSetNameIfNotSet(
        title, StyleEngine::DontUpdateActiveSheets);
  }

  bool mediaQueryMatches = true;
  LocalFrame* frame = loadingFrame();
  if (!m_owner->media().isEmpty() && frame) {
    MediaQuerySet* media = MediaQuerySet::create(m_owner->media());
    MediaQueryEvaluator evaluator(frame);
    mediaQueryMatches = evaluator.eval(media);
  }

  // Don't hold up layout tree construction and script execution on
  // stylesheets that are not needed for the layout at the moment.
  bool blocking = mediaQueryMatches && !m_owner->isAlternate() &&
                  m_owner->isCreatedByParser();
  addPendingSheet(blocking ? Blocking : NonBlocking);

  // Load stylesheets that are not needed for the layout immediately with low
  // priority.  When the link element is created by scripts, load the
  // stylesheets asynchronously but in high priority.
  bool lowPriority = !mediaQueryMatches || m_owner->isAlternate();
  FetchRequest request = builder.build(lowPriority);
  CrossOriginAttributeValue crossOrigin = crossOriginAttributeValue(
      m_owner->fastGetAttribute(HTMLNames::crossoriginAttr));
  if (crossOrigin != CrossOriginAttributeNotSet) {
    request.setCrossOriginAccessControl(document().getSecurityOrigin(),
                                        crossOrigin);
    setFetchFollowingCORS();
  }

  String integrityAttr = m_owner->fastGetAttribute(HTMLNames::integrityAttr);
  if (!integrityAttr.isEmpty()) {
    IntegrityMetadataSet metadataSet;
    SubresourceIntegrity::parseIntegrityAttribute(integrityAttr, metadataSet);
    request.setIntegrityMetadata(metadataSet);
  }
  setResource(CSSStyleSheetResource::fetch(request, document().fetcher()));

  if (m_loading && !resource()) {
    // The request may have been denied if (for example) the stylesheet is
    // local and the document is remote, or if there was a Content Security
    // Policy Failure.  setCSSStyleSheet() can be called synchronuosly in
    // setResource() and thus resource() is null and |m_loading| is false in
    // such cases even if the request succeeds.
    m_loading = false;
    removePendingSheet();
    notifyLoadedSheetAndAllCriticalSubresources(
        Node::ErrorOccurredLoadingSubresource);
  }
  return Loaded;
}

void LinkStyle::process() {
  DCHECK(m_owner->shouldProcessStyle());
  String type = m_owner->typeValue().lower();
  String as = m_owner->asValue().lower();
  String media = m_owner->media().lower();
  LinkRequestBuilder builder(m_owner);

  if (m_owner->relAttribute().getIconType() != InvalidIcon &&
      builder.url().isValid() && !builder.url().isEmpty()) {
    if (!m_owner->shouldLoadLink())
      return;
    if (!document().getSecurityOrigin()->canDisplay(builder.url()))
      return;
    if (!document().contentSecurityPolicy()->allowImageFromSource(
            builder.url()))
      return;
    if (document().frame() && document().frame()->loader().client()) {
      document().frame()->loader().client()->dispatchDidChangeIcons(
          m_owner->relAttribute().getIconType());
    }
  }

  if (!m_owner->loadLink(type, as, media, m_owner->referrerPolicy(),
                         builder.url()))
    return;

  if (loadStylesheetIfNeeded(builder, type) == NotNeeded && m_sheet) {
    // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
    StyleSheet* removedSheet = m_sheet.get();
    clearSheet();
    document().styleEngine().setNeedsActiveStyleUpdate(removedSheet,
                                                       FullStyleUpdate);
  }
}

void LinkStyle::setSheetTitle(
    const String& title,
    StyleEngine::ActiveSheetsUpdate updateActiveSheets) {
  if (!m_owner->isInDocumentTree() || !m_owner->relAttribute().isStyleSheet())
    return;

  if (m_sheet)
    m_sheet->setTitle(title);

  if (title.isEmpty() || !isUnset() || m_owner->isAlternate())
    return;

  KURL href = m_owner->getNonEmptyURLAttribute(hrefAttr);
  if (href.isValid() && !href.isEmpty()) {
    document().styleEngine().setPreferredStylesheetSetNameIfNotSet(
        title, updateActiveSheets);
  }
}

void LinkStyle::ownerRemoved() {
  if (m_sheet)
    clearSheet();

  if (styleSheetIsLoading())
    removePendingSheet();
}

DEFINE_TRACE(LinkStyle) {
  visitor->trace(m_sheet);
  LinkResource::trace(visitor);
  ResourceOwner<StyleSheetResource>::trace(visitor);
}

}  // namespace blink
