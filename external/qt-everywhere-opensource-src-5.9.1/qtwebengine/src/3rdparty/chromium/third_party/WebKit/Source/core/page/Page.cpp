/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All
 * Rights Reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/page/Page.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/VisitedLinkState.h"
#include "core/editing/DragCaretController.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/Event.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLMediaElement.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/TextAutosizer.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/FocusController.h"
#include "core/page/PointerLockController.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/page/ValidationMessageClient.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/plugins/PluginData.h"
#include "public/platform/Platform.h"

namespace blink {

// Set of all live pages; includes internal Page objects that are
// not observable from scripts.
static Page::PageSet& allPages() {
  DEFINE_STATIC_LOCAL(Page::PageSet, pages, ());
  return pages;
}

Page::PageSet& Page::ordinaryPages() {
  DEFINE_STATIC_LOCAL(Page::PageSet, pages, ());
  return pages;
}

void Page::networkStateChanged(bool online) {
  HeapVector<Member<LocalFrame>> frames;

  // Get all the frames of all the pages in all the page groups
  for (Page* page : allPages()) {
    for (Frame* frame = page->mainFrame(); frame;
         frame = frame->tree().traverseNext()) {
      // FIXME: There is currently no way to dispatch events to out-of-process
      // frames.
      if (frame->isLocalFrame())
        frames.append(toLocalFrame(frame));
    }
  }

  AtomicString eventName =
      online ? EventTypeNames::online : EventTypeNames::offline;
  for (const auto& frame : frames) {
    frame->domWindow()->dispatchEvent(Event::create(eventName));
    InspectorInstrumentation::networkStateChanged(frame.get(), online);
  }
}

float deviceScaleFactor(LocalFrame* frame) {
  if (!frame)
    return 1;
  Page* page = frame->page();
  if (!page)
    return 1;
  return page->deviceScaleFactor();
}

Page* Page::createOrdinary(PageClients& pageClients) {
  Page* page = create(pageClients);
  ordinaryPages().add(page);
  if (ScopedPageSuspender::isActive())
    page->setSuspended(true);
  return page;
}

Page::Page(PageClients& pageClients)
    : SettingsDelegate(Settings::create()),
      m_animator(PageAnimator::create(*this)),
      m_autoscrollController(AutoscrollController::create(*this)),
      m_chromeClient(pageClients.chromeClient),
      m_dragCaretController(DragCaretController::create()),
      m_dragController(DragController::create(this)),
      m_focusController(FocusController::create(this)),
      m_contextMenuController(
          ContextMenuController::create(this, pageClients.contextMenuClient)),
      m_pointerLockController(PointerLockController::create(this)),
      m_mainFrame(nullptr),
      m_editorClient(pageClients.editorClient),
      m_spellCheckerClient(pageClients.spellCheckerClient),
      m_useCounter(pageClients.chromeClient &&
                           pageClients.chromeClient->isSVGImageChromeClient()
                       ? UseCounter::SVGImageContext
                       : UseCounter::DefaultContext),
      m_openedByDOM(false),
      m_tabKeyCyclesThroughElements(true),
      m_suspended(false),
      m_deviceScaleFactor(1),
      m_visibilityState(PageVisibilityStateVisible),
      m_isCursorVisible(true),
#if ENABLE(ASSERT)
      m_isPainting(false),
#endif
      m_frameHost(FrameHost::create(*this)) {
  ASSERT(m_editorClient);

  ASSERT(!allPages().contains(this));
  allPages().add(this);
}

Page::~Page() {
  // willBeDestroyed() must be called before Page destruction.
  ASSERT(!m_mainFrame);
}

void Page::closeSoon() {
  // Make sure this Page can no longer be found by JS.
  m_isClosing = true;

  // TODO(dcheng): Try to remove this in a followup, it's not obviously needed.
  if (m_mainFrame->isLocalFrame())
    toLocalFrame(m_mainFrame)->loader().stopAllLoaders();

  chromeClient().closeWindowSoon();
}

ViewportDescription Page::viewportDescription() const {
  return mainFrame() && mainFrame()->isLocalFrame() &&
                 deprecatedLocalMainFrame()->document()
             ? deprecatedLocalMainFrame()->document()->viewportDescription()
             : ViewportDescription();
}

ScrollingCoordinator* Page::scrollingCoordinator() {
  if (!m_scrollingCoordinator && m_settings->acceleratedCompositingEnabled())
    m_scrollingCoordinator = ScrollingCoordinator::create(this);

  return m_scrollingCoordinator.get();
}

String Page::mainThreadScrollingReasonsAsText() {
  if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
    return scrollingCoordinator->mainThreadScrollingReasonsAsText();

  return String();
}

ClientRectList* Page::nonFastScrollableRects(const LocalFrame* frame) {
  if (ScrollingCoordinator* scrollingCoordinator =
          this->scrollingCoordinator()) {
    // Hits in compositing/iframes/iframe-composited-scrolling.html
    DisableCompositingQueryAsserts disabler;
    scrollingCoordinator->updateAfterCompositingChangeIfNeeded();
  }

  if (!frame->view()->layerForScrolling())
    return ClientRectList::create();

  // Now retain non-fast scrollable regions
  return ClientRectList::create(frame->view()
                                    ->layerForScrolling()
                                    ->platformLayer()
                                    ->nonFastScrollableRegion());
}

void Page::setMainFrame(Frame* mainFrame) {
  // Should only be called during initialization or swaps between local and
  // remote frames.
  // FIXME: Unfortunately we can't assert on this at the moment, because this
  // is called in the base constructor for both LocalFrame and RemoteFrame,
  // when the vtables for the derived classes have not yet been setup.
  m_mainFrame = mainFrame;
}

void Page::willUnloadDocument(const Document& document) {
  if (m_validationMessageClient)
    m_validationMessageClient->willUnloadDocument(document);
}

void Page::documentDetached(Document* document) {
  m_pointerLockController->documentDetached(document);
  m_contextMenuController->documentDetached(document);
  if (m_validationMessageClient)
    m_validationMessageClient->documentDetached(*document);
  m_hostsUsingFeatures.documentDetached(*document);
}

bool Page::openedByDOM() const {
  return m_openedByDOM;
}

void Page::setOpenedByDOM() {
  m_openedByDOM = true;
}

void Page::platformColorsChanged() {
  for (const Page* page : allPages())
    for (Frame* frame = page->mainFrame(); frame;
         frame = frame->tree().traverseNext()) {
      if (frame->isLocalFrame())
        toLocalFrame(frame)->document()->platformColorsChanged();
    }
}

void Page::setNeedsRecalcStyleInAllFrames() {
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (frame->isLocalFrame())
      toLocalFrame(frame)->document()->setNeedsStyleRecalc(
          SubtreeStyleChange,
          StyleChangeReasonForTracing::create(StyleChangeReason::Settings));
  }
}

void Page::refreshPlugins() {
  PluginData::refreshBrowserSidePluginCache();

  for (const Page* page : allPages()) {
    // Clear out the page's plugin data.
    if (page->m_pluginData)
      page->m_pluginData = nullptr;
  }
}

PluginData* Page::pluginData(SecurityOrigin* mainFrameOrigin) const {
  if (!m_pluginData ||
      !mainFrameOrigin->isSameSchemeHostPort(m_pluginData->origin()))
    m_pluginData = PluginData::create(mainFrameOrigin);
  return m_pluginData.get();
}

void Page::setValidationMessageClient(ValidationMessageClient* client) {
  m_validationMessageClient = client;
}

void Page::setSuspended(bool suspend) {
  if (suspend == m_suspended)
    return;

  m_suspended = suspend;
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (frame->isLocalFrame())
      toLocalFrame(frame)->loader().setDefersLoading(suspend);
  }
}

void Page::setPageScaleFactor(float scale) {
  frameHost().visualViewport().setScale(scale);
}

float Page::pageScaleFactor() const {
  return frameHost().visualViewport().scale();
}

void Page::setDeviceScaleFactor(float scaleFactor) {
  if (m_deviceScaleFactor == scaleFactor)
    return;

  m_deviceScaleFactor = scaleFactor;

  if (mainFrame() && mainFrame()->isLocalFrame())
    deprecatedLocalMainFrame()->deviceScaleFactorChanged();
}

void Page::allVisitedStateChanged(bool invalidateVisitedLinkHashes) {
  for (const Page* page : ordinaryPages()) {
    for (Frame* frame = page->m_mainFrame; frame;
         frame = frame->tree().traverseNext()) {
      if (frame->isLocalFrame())
        toLocalFrame(frame)
            ->document()
            ->visitedLinkState()
            .invalidateStyleForAllLinks(invalidateVisitedLinkHashes);
    }
  }
}

void Page::visitedStateChanged(LinkHash linkHash) {
  for (const Page* page : ordinaryPages()) {
    for (Frame* frame = page->m_mainFrame; frame;
         frame = frame->tree().traverseNext()) {
      if (frame->isLocalFrame())
        toLocalFrame(frame)
            ->document()
            ->visitedLinkState()
            .invalidateStyleForLink(linkHash);
    }
  }
}

void Page::setVisibilityState(PageVisibilityState visibilityState,
                              bool isInitialState) {
  if (m_visibilityState == visibilityState)
    return;
  m_visibilityState = visibilityState;

  if (!isInitialState)
    notifyPageVisibilityChanged();

  if (!isInitialState && m_mainFrame)
    m_mainFrame->didChangeVisibilityState();
}

PageVisibilityState Page::visibilityState() const {
  return m_visibilityState;
}

bool Page::isPageVisible() const {
  return visibilityState() == PageVisibilityStateVisible;
}

bool Page::isCursorVisible() const {
  return m_isCursorVisible && settings().deviceSupportsMouse();
}

void Page::settingsChanged(SettingsDelegate::ChangeType changeType) {
  switch (changeType) {
    case SettingsDelegate::StyleChange:
      setNeedsRecalcStyleInAllFrames();
      break;
    case SettingsDelegate::ViewportDescriptionChange:
      if (mainFrame() && mainFrame()->isLocalFrame()) {
        deprecatedLocalMainFrame()->document()->updateViewportDescription();
        // The text autosizer has dependencies on the viewport.
        if (TextAutosizer* textAutosizer =
                deprecatedLocalMainFrame()->document()->textAutosizer())
          textAutosizer->updatePageInfoInAllFrames();
      }
      break;
    case SettingsDelegate::DNSPrefetchingChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
          toLocalFrame(frame)->document()->initDNSPrefetch();
      }
      break;
    case SettingsDelegate::ImageLoadingChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame()) {
          toLocalFrame(frame)->document()->fetcher()->setImagesEnabled(
              settings().imagesEnabled());
          toLocalFrame(frame)->document()->fetcher()->setAutoLoadImages(
              settings().loadsImagesAutomatically());
        }
      }
      break;
    case SettingsDelegate::TextAutosizingChange:
      if (!mainFrame() || !mainFrame()->isLocalFrame())
        break;
      if (TextAutosizer* textAutosizer =
              deprecatedLocalMainFrame()->document()->textAutosizer())
        textAutosizer->updatePageInfoInAllFrames();
      break;
    case SettingsDelegate::FontFamilyChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
          toLocalFrame(frame)
              ->document()
              ->styleEngine()
              .updateGenericFontFamilySettings();
      }
      break;
    case SettingsDelegate::AcceleratedCompositingChange:
      updateAcceleratedCompositingSettings();
      break;
    case SettingsDelegate::MediaQueryChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
          toLocalFrame(frame)->document()->mediaQueryAffectingValueChanged();
      }
      break;
    case SettingsDelegate::AccessibilityStateChange:
      if (!mainFrame() || !mainFrame()->isLocalFrame())
        break;
      deprecatedLocalMainFrame()
          ->document()
          ->axObjectCacheOwner()
          .clearAXObjectCache();
      break;
    case SettingsDelegate::ViewportRuleChange: {
      if (!mainFrame() || !mainFrame()->isLocalFrame())
        break;
      if (Document* doc = toLocalFrame(mainFrame())->document())
        doc->styleEngine().viewportRulesChanged();
    } break;
    case SettingsDelegate::TextTrackKindUserPreferenceChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame()) {
          Document* doc = toLocalFrame(frame)->document();
          if (doc)
            HTMLMediaElement::setTextTrackKindUserPreferenceForAllMediaElements(
                doc);
        }
      }
      break;
    case SettingsDelegate::DOMWorldsChange: {
      if (!settings().forceMainWorldInitialization())
        break;
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
          continue;
        LocalFrame* localFrame = toLocalFrame(frame);
        if (localFrame->loader()
                .stateMachine()
                ->committedFirstRealDocumentLoad()) {
          localFrame->script().initializeMainWorld();
        }
      }
    } break;
  }
}

void Page::updateAcceleratedCompositingSettings() {
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (FrameView* view = toLocalFrame(frame)->view())
      view->updateAcceleratedCompositingSettings();
  }
}

void Page::didCommitLoad(LocalFrame* frame) {
  if (m_mainFrame == frame) {
    // TODO(rbyers): Most of this doesn't appear to take into account that each
    // SVGImage gets it's own Page instance.
    frameHost().consoleMessageStorage().clear();
    useCounter().didCommitLoad();
    deprecation().clearSuppression();
    frameHost().visualViewport().sendUMAMetrics();

    // Need to reset visual viewport position here since before commit load we
    // would update the previous history item, Page::didCommitLoad is called
    // after a new history item is created in FrameLoader.
    // See crbug.com/642279
    frameHost().visualViewport().setScrollOffset(ScrollOffset(),
                                                 ProgrammaticScroll);
    m_hostsUsingFeatures.updateMeasurementsAndClear();
  }
}

void Page::acceptLanguagesChanged() {
  HeapVector<Member<LocalFrame>> frames;

  // Even though we don't fire an event from here, the LocalDOMWindow's will
  // fire an event so we keep the frames alive until we are done.
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (frame->isLocalFrame())
      frames.append(toLocalFrame(frame));
  }

  for (unsigned i = 0; i < frames.size(); ++i)
    frames[i]->localDOMWindow()->acceptLanguagesChanged();
}

DEFINE_TRACE(Page) {
  visitor->trace(m_animator);
  visitor->trace(m_autoscrollController);
  visitor->trace(m_chromeClient);
  visitor->trace(m_dragCaretController);
  visitor->trace(m_dragController);
  visitor->trace(m_focusController);
  visitor->trace(m_contextMenuController);
  visitor->trace(m_pointerLockController);
  visitor->trace(m_scrollingCoordinator);
  visitor->trace(m_mainFrame);
  visitor->trace(m_validationMessageClient);
  visitor->trace(m_frameHost);
  Supplementable<Page>::trace(visitor);
  PageVisibilityNotifier::trace(visitor);
}

void Page::layerTreeViewInitialized(WebLayerTreeView& layerTreeView) {
  if (scrollingCoordinator())
    scrollingCoordinator()->layerTreeViewInitialized(layerTreeView);
}

void Page::willCloseLayerTreeView(WebLayerTreeView& layerTreeView) {
  if (m_scrollingCoordinator)
    m_scrollingCoordinator->willCloseLayerTreeView(layerTreeView);
}

void Page::willBeDestroyed() {
  Frame* mainFrame = m_mainFrame;

  mainFrame->detach(FrameDetachType::Remove);

  ASSERT(allPages().contains(this));
  allPages().remove(this);
  ordinaryPages().remove(this);

  if (m_scrollingCoordinator)
    m_scrollingCoordinator->willBeDestroyed();

  chromeClient().chromeDestroyed();
  if (m_validationMessageClient)
    m_validationMessageClient->willBeDestroyed();
  m_mainFrame = nullptr;

  PageVisibilityNotifier::notifyContextDestroyed();
}

Page::PageClients::PageClients()
    : chromeClient(nullptr),
      contextMenuClient(nullptr),
      editorClient(nullptr),
      spellCheckerClient(nullptr) {}

Page::PageClients::~PageClients() {}

template class CORE_TEMPLATE_EXPORT Supplement<Page>;

}  // namespace blink
