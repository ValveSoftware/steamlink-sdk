/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Internals_h
#define Internals_h

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Animation;
class CallbackFunctionTest;
class CanvasRenderingContext;
class ClientRect;
class ClientRectList;
class DOMArrayBuffer;
class DOMPoint;
class DictionaryTest;
class Document;
class DocumentMarker;
class Element;
class ExceptionState;
class GCObservation;
class HTMLInputElement;
class HTMLMediaElement;
class HTMLSelectElement;
class InternalRuntimeFlags;
class InternalSettings;
class LayerRectList;
class LocalDOMWindow;
class LocalFrame;
class Location;
class Node;
class OriginTrialsTest;
class Page;
class PrivateScriptTest;
class Range;
class SerializedScriptValue;
class ShadowRoot;
class TypeConversions;
class UnionTypesTest;
class ScrollState;
template <typename NodeType>
class StaticNodeTypeList;
using StaticNodeList = StaticNodeTypeList<Node>;

class Internals final : public GarbageCollectedFinalized<Internals>,
                        public ScriptWrappable,
                        public ContextLifecycleObserver,
                        public ValueIterable<int> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Internals);

 public:
  static Internals* create(ExecutionContext* context) {
    return new Internals(context);
  }
  virtual ~Internals();

  static void resetToConsistentState(Page*);

  String elementLayoutTreeAsText(Element*, ExceptionState&);

  String address(Node*);

  GCObservation* observeGC(ScriptValue);

  bool isPreloaded(const String& url);
  bool isPreloadedBy(const String& url, Document*);
  bool isLoadingFromMemoryCache(const String& url);
  int getResourcePriority(const String& url, Document*);
  String getResourceHeader(const String& url, const String& header, Document*);

  bool isSharingStyle(Element*, Element*) const;

  CSSStyleDeclaration* computedStyleIncludingVisitedInfo(Node*) const;

  ShadowRoot* createUserAgentShadowRoot(Element* host);

  ShadowRoot* shadowRoot(Element* host);
  ShadowRoot* youngestShadowRoot(Element* host);
  ShadowRoot* oldestShadowRoot(Element* host);
  ShadowRoot* youngerShadowRoot(Node* shadow, ExceptionState&);
  String shadowRootType(const Node*, ExceptionState&) const;
  bool hasShadowInsertionPoint(const Node*, ExceptionState&) const;
  bool hasContentElement(const Node*, ExceptionState&) const;
  size_t countElementShadow(const Node*, ExceptionState&) const;
  const AtomicString& shadowPseudoId(Element*);

  // Animation testing.
  void pauseAnimations(double pauseTime, ExceptionState&);
  bool isCompositedAnimation(Animation*);
  void disableCompositedAnimation(Animation*);
  void disableCSSAdditiveAnimations();

  // Modifies m_desiredFrameStartTime in BitmapImage to advance the next frame
  // time for testing whether animated images work properly.
  void advanceTimeForImage(Element* image,
                           double deltaTimeInSeconds,
                           ExceptionState&);

  // Advances an animated image. For BitmapImage (e.g., animated gifs) this
  // will advance to the next frame. For SVGImage, this will trigger an
  // animation update for CSS and advance the SMIL timeline by one frame.
  void advanceImageAnimation(Element* image, ExceptionState&);

  bool isValidContentSelect(Element* insertionPoint, ExceptionState&);
  Node* treeScopeRootNode(Node*);
  Node* parentTreeScope(Node*);
  bool hasSelectorForIdInShadow(Element* host,
                                const AtomicString& idValue,
                                ExceptionState&);
  bool hasSelectorForClassInShadow(Element* host,
                                   const AtomicString& className,
                                   ExceptionState&);
  bool hasSelectorForAttributeInShadow(Element* host,
                                       const AtomicString& attributeName,
                                       ExceptionState&);
  unsigned short compareTreeScopePosition(const Node*,
                                          const Node*,
                                          ExceptionState&) const;

  Node* nextSiblingInFlatTree(Node*, ExceptionState&);
  Node* firstChildInFlatTree(Node*, ExceptionState&);
  Node* lastChildInFlatTree(Node*, ExceptionState&);
  Node* nextInFlatTree(Node*, ExceptionState&);
  Node* previousInFlatTree(Node*, ExceptionState&);

  unsigned updateStyleAndReturnAffectedElementCount(ExceptionState&) const;
  unsigned needsLayoutCount(ExceptionState&) const;
  unsigned hitTestCount(Document*, ExceptionState&) const;
  unsigned hitTestCacheHits(Document*, ExceptionState&) const;
  Element* elementFromPoint(Document*,
                            double x,
                            double y,
                            bool ignoreClipping,
                            bool allowChildFrameContent,
                            ExceptionState&) const;
  void clearHitTestCache(Document*, ExceptionState&) const;

  String visiblePlaceholder(Element*);
  void selectColorInColorChooser(Element*, const String& colorValue);
  void endColorChooser(Element*);
  bool hasAutofocusRequest(Document*);
  bool hasAutofocusRequest();
  Vector<String> formControlStateOfHistoryItem(ExceptionState&);
  void setFormControlStateOfHistoryItem(const Vector<String>&, ExceptionState&);
  DOMWindow* pagePopupWindow() const;

  ClientRect* absoluteCaretBounds(ExceptionState&);

  ClientRect* boundingBox(Element*);

  void setMarker(Document*, const Range*, const String&, ExceptionState&);
  unsigned markerCountForNode(Node*, const String&, ExceptionState&);
  unsigned activeMarkerCountForNode(Node*);
  Range* markerRangeForNode(Node*,
                            const String& markerType,
                            unsigned index,
                            ExceptionState&);
  String markerDescriptionForNode(Node*,
                                  const String& markerType,
                                  unsigned index,
                                  ExceptionState&);
  void addTextMatchMarker(const Range*, bool isActive);
  void addCompositionMarker(const Range*,
                            const String& underlineColorValue,
                            bool thick,
                            const String& backgroundColorValue,
                            ExceptionState&);
  void setMarkersActive(Node*, unsigned startOffset, unsigned endOffset, bool);
  void setMarkedTextMatchesAreHighlighted(Document*, bool);

  void setFrameViewPosition(Document*, long x, long y, ExceptionState&);
  String viewportAsText(Document*,
                        float devicePixelRatio,
                        int availableWidth,
                        int availableHeight,
                        ExceptionState&);

  bool elementShouldAutoComplete(Element* inputElement, ExceptionState&);
  String suggestedValue(Element*, ExceptionState&);
  void setSuggestedValue(Element*, const String&, ExceptionState&);
  void setEditingValue(Element* inputElement, const String&, ExceptionState&);
  void setAutofilled(Element*, bool enabled, ExceptionState&);

  Range* rangeFromLocationAndLength(Element* scope,
                                    int rangeLocation,
                                    int rangeLength);
  unsigned locationFromRange(Element* scope, const Range*);
  unsigned lengthFromRange(Element* scope, const Range*);
  String rangeAsText(const Range*);

  DOMPoint* touchPositionAdjustedToBestClickableNode(long x,
                                                     long y,
                                                     long width,
                                                     long height,
                                                     Document*,
                                                     ExceptionState&);
  Node* touchNodeAdjustedToBestClickableNode(long x,
                                             long y,
                                             long width,
                                             long height,
                                             Document*,
                                             ExceptionState&);
  DOMPoint* touchPositionAdjustedToBestContextMenuNode(long x,
                                                       long y,
                                                       long width,
                                                       long height,
                                                       Document*,
                                                       ExceptionState&);
  Node* touchNodeAdjustedToBestContextMenuNode(long x,
                                               long y,
                                               long width,
                                               long height,
                                               Document*,
                                               ExceptionState&);
  ClientRect* bestZoomableAreaForTouchPoint(long x,
                                            long y,
                                            long width,
                                            long height,
                                            Document*,
                                            ExceptionState&);

  int lastSpellCheckRequestSequence(Document*, ExceptionState&);
  int lastSpellCheckProcessedSequence(Document*, ExceptionState&);

  Vector<AtomicString> userPreferredLanguages() const;
  void setUserPreferredLanguages(const Vector<String>&);

  unsigned activeDOMObjectCount(Document*);
  unsigned wheelEventHandlerCount(Document*);
  unsigned scrollEventHandlerCount(Document*);
  unsigned touchStartOrMoveEventHandlerCount(Document*);
  unsigned touchEndOrCancelEventHandlerCount(Document*);
  LayerRectList* touchEventTargetLayerRects(Document*, ExceptionState&);

  bool executeCommand(Document*,
                      const String& name,
                      const String& value,
                      ExceptionState&);

  AtomicString htmlNamespace();
  Vector<AtomicString> htmlTags();
  AtomicString svgNamespace();
  Vector<AtomicString> svgTags();

  // This is used to test rect based hit testing like what's done on touch
  // screens.
  StaticNodeList* nodesFromRect(Document*,
                                int x,
                                int y,
                                unsigned topPadding,
                                unsigned rightPadding,
                                unsigned bottomPadding,
                                unsigned leftPadding,
                                bool ignoreClipping,
                                bool allowChildFrameContent,
                                ExceptionState&) const;

  bool hasSpellingMarker(Document*, int from, int length, ExceptionState&);
  bool hasGrammarMarker(Document*, int from, int length, ExceptionState&);
  void setSpellCheckingEnabled(bool, ExceptionState&);
  void replaceMisspelled(Document*, const String&, ExceptionState&);

  bool canHyphenate(const AtomicString& locale);
  void setMockHyphenation(const AtomicString& locale);

  bool isOverwriteModeEnabled(Document*);
  void toggleOverwriteModeEnabled(Document*);

  unsigned numberOfScrollableAreas(Document*);

  bool isPageBoxVisible(Document*, int pageNumber);

  InternalSettings* settings() const;
  InternalRuntimeFlags* runtimeFlags() const;
  unsigned workerThreadCount() const;

  void setDeviceProximity(Document*,
                          const String& eventType,
                          double value,
                          double min,
                          double max,
                          ExceptionState&);

  String layerTreeAsText(Document*, unsigned flags, ExceptionState&) const;
  String layerTreeAsText(Document*, ExceptionState&) const;
  String elementLayerTreeAsText(Element*,
                                unsigned flags,
                                ExceptionState&) const;
  String elementLayerTreeAsText(Element*, ExceptionState&) const;

  bool scrollsWithRespectTo(Element*, Element*, ExceptionState&);

  String scrollingStateTreeAsText(Document*) const;
  String mainThreadScrollingReasons(Document*, ExceptionState&) const;
  ClientRectList* nonFastScrollableRects(Document*, ExceptionState&) const;

  void evictAllResources() const;

  unsigned numberOfLiveNodes() const;
  unsigned numberOfLiveDocuments() const;
  String dumpRefCountedInstanceCounts() const;
  LocalDOMWindow* openDummyInspectorFrontend(const String& url);
  void closeDummyInspectorFrontend();

  String counterValue(Element*);

  int pageNumber(Element*, float pageWidth, float pageHeight, ExceptionState&);
  Vector<String> shortcutIconURLs(Document*) const;
  Vector<String> allIconURLs(Document*) const;

  int numberOfPages(float pageWidthInPixels,
                    float pageHeightInPixels,
                    ExceptionState&);
  String pageProperty(String, int, ExceptionState& = ASSERT_NO_EXCEPTION) const;
  String pageSizeAndMarginsInPixels(
      int,
      int,
      int,
      int,
      int,
      int,
      int,
      ExceptionState& = ASSERT_NO_EXCEPTION) const;

  float pageScaleFactor(ExceptionState&);
  void setPageScaleFactor(float scaleFactor, ExceptionState&);
  void setPageScaleFactorLimits(float minScaleFactor,
                                float maxScaleFactor,
                                ExceptionState&);

  bool magnifyScaleAroundAnchor(float factor, float x, float y);

  void setIsCursorVisible(Document*, bool, ExceptionState&);

  String effectivePreload(HTMLMediaElement*);

  void mediaPlayerRemoteRouteAvailabilityChanged(HTMLMediaElement*, bool);
  void mediaPlayerPlayingRemotelyChanged(HTMLMediaElement*, bool);

  void registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme);
  void registerURLSchemeAsBypassingContentSecurityPolicy(
      const String& scheme,
      const Vector<String>& policyAreas);
  void removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      const String& scheme);

  TypeConversions* typeConversions() const;
  PrivateScriptTest* privateScriptTest() const;
  DictionaryTest* dictionaryTest() const;
  UnionTypesTest* unionTypesTest() const;
  OriginTrialsTest* originTrialsTest() const;
  CallbackFunctionTest* callbackFunctionTest() const;

  Vector<String> getReferencedFilePaths() const;

  void startTrackingRepaints(Document*, ExceptionState&);
  void stopTrackingRepaints(Document*, ExceptionState&);
  void updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(
      Node*,
      ExceptionState&);
  void forceFullRepaint(Document*, ExceptionState&);

  ClientRectList* draggableRegions(Document*, ExceptionState&);
  ClientRectList* nonDraggableRegions(Document*, ExceptionState&);

  DOMArrayBuffer* serializeObject(PassRefPtr<SerializedScriptValue>) const;
  PassRefPtr<SerializedScriptValue> deserializeBuffer(DOMArrayBuffer*) const;

  String getCurrentCursorInfo();

  bool cursorUpdatePending() const;

  String markerTextForListItem(Element*);

  void forceReload(bool bypassCache);

  String getImageSourceURL(Element*);

  String selectMenuListText(HTMLSelectElement*);
  bool isSelectPopupVisible(Node*);
  bool selectPopupItemStyleIsRtl(Node*, int);
  int selectPopupItemStyleFontHeight(Node*, int);
  void resetTypeAheadSession(HTMLSelectElement*);

  ClientRect* selectionBounds(ExceptionState&);

  bool loseSharedGraphicsContext3D();

  void forceCompositingUpdate(Document*, ExceptionState&);

  void setZoomFactor(float);

  void setShouldRevealPassword(Element*, bool, ExceptionState&);

  ScriptPromise createResolvedPromise(ScriptState*, ScriptValue);
  ScriptPromise createRejectedPromise(ScriptState*, ScriptValue);
  ScriptPromise addOneToPromise(ScriptState*, ScriptPromise);
  ScriptPromise promiseCheck(ScriptState*,
                             long,
                             bool,
                             const Dictionary&,
                             const String&,
                             const Vector<String>&,
                             ExceptionState&);
  ScriptPromise promiseCheckWithoutExceptionState(ScriptState*,
                                                  const Dictionary&,
                                                  const String&,
                                                  const Vector<String>&);
  ScriptPromise promiseCheckRange(ScriptState*, long);
  ScriptPromise promiseCheckOverload(ScriptState*, Location*);
  ScriptPromise promiseCheckOverload(ScriptState*, Document*);
  ScriptPromise promiseCheckOverload(ScriptState*, Location*, long, long);

  DECLARE_TRACE();

  void setValueForUser(HTMLInputElement*, const String&);

  String textSurroundingNode(Node*, int x, int y, unsigned long maxLength);

  void setFocused(bool);
  void setInitialFocus(bool);

  bool ignoreLayoutWithPendingStylesheets(Document*);

  void setNetworkConnectionInfoOverride(bool,
                                        const String&,
                                        double downlinkMaxMbps,
                                        ExceptionState&);
  void clearNetworkConnectionInfoOverride();

  unsigned countHitRegions(CanvasRenderingContext*);

  bool isInCanvasFontCache(Document*, const String&);
  unsigned canvasFontCacheMaxFonts();

  void setScrollChain(ScrollState*,
                      const HeapVector<Member<Element>>& elements,
                      ExceptionState&);

  // Schedule a forced Blink GC run (Oilpan) at the end of event loop.
  // Note: This is designed to be only used from PerformanceTests/BlinkGC to
  //       explicitly measure only Blink GC time.  Normal LayoutTests should use
  //       gc() instead as it would trigger both Blink GC and V8 GC.
  void forceBlinkGCWithoutV8GC();

  String selectedHTMLForClipboard();
  String selectedTextForClipboard();

  void setVisualViewportOffset(int x, int y);
  int visualViewportHeight();
  int visualViewportWidth();
  // The scroll position of the visual viewport relative to the document origin.
  float visualViewportScrollX();
  float visualViewportScrollY();

  // Return true if the given use counter exists for the given document.
  // |useCounterId| must be one of the values from the UseCounter::Feature enum.
  bool isUseCounted(Document*, int useCounterId);
  bool isCSSPropertyUseCounted(Document*, const String&);

  String unscopableAttribute();
  String unscopableMethod();

  ClientRectList* focusRingRects(Element*);
  ClientRectList* outlineRects(Element*);

  void setCapsLockState(bool enabled);

  bool setScrollbarVisibilityInScrollableArea(Node*, bool visible);

  // Translate given platform monotonic time in seconds to high resolution
  // document time in seconds
  double monotonicTimeToZeroBasedDocumentTime(double, ExceptionState&);

  void setMediaElementNetworkState(HTMLMediaElement*, int state);

  // TODO(liberato): remove once autoplay gesture override experiment concludes.
  void triggerAutoplayViewportCheck(HTMLMediaElement*);

  // Returns the run state of the node's scroll animator (see
  // ScrollAnimatorCompositorCoordinater::RunState), or -1 if the node does not
  // have a scrollable area.
  String getScrollAnimationState(Node*) const;

  // Returns the run state of the node's programmatic scroll animator (see
  // ScrollAnimatorCompositorCoordinater::RunState), or -1 if the node does not
  // have a scrollable area.
  String getProgrammaticScrollAnimationState(Node*) const;

  // Returns the visual rect of a node's LayoutObject.
  ClientRect* visualRect(Node*);

  // Intentional crash.
  void crash();

  // Overrides if the device is low-end (low on memory).
  void setIsLowEndDevice(bool);

 private:
  explicit Internals(ExecutionContext*);
  Document* contextDocument() const;
  LocalFrame* frame() const;
  Vector<String> iconURLs(Document*, int iconTypesMask) const;
  ClientRectList* annotatedRegions(Document*, bool draggable, ExceptionState&);

  DocumentMarker* markerAt(Node*,
                           const String& markerType,
                           unsigned index,
                           ExceptionState&);
  Member<InternalRuntimeFlags> m_runtimeFlags;

  IterationSource* startIteration(ScriptState*, ExceptionState&) override;
};

}  // namespace blink

#endif  // Internals_h
