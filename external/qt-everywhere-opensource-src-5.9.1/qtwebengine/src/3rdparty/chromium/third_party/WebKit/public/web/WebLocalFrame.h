// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLocalFrame_h
#define WebLocalFrame_h

#include "WebCompositionUnderline.h"
#include "WebFrame.h"
#include "WebFrameLoadType.h"
#include "WebHistoryItem.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLError.h"

namespace blink {

class WebAutofillClient;
class WebContentSettingsClient;
class WebDevToolsAgent;
class WebDevToolsAgentClient;
class WebDoubleSize;
class WebFrameClient;
class WebFrameWidget;
class WebRange;
class WebScriptExecutionCallback;
class WebSuspendableTask;
enum class WebCachePolicy;
enum class WebSandboxFlags;
enum class WebTreeScopeType;
struct WebFindOptions;
struct WebFloatRect;
struct WebPrintPresetOptions;

// Interface for interacting with in process frames. This contains methods that
// require interacting with a frame's document.
// FIXME: Move lots of methods from WebFrame in here.
class WebLocalFrame : public WebFrame {
 public:
  // Creates a WebFrame. Delete this WebFrame by calling WebFrame::close().
  // WebFrameClient may not be null.
  BLINK_EXPORT static WebLocalFrame* create(WebTreeScopeType,
                                            WebFrameClient*,
                                            WebFrame* opener = nullptr);

  // Used to create a provisional local frame in prepration for replacing a
  // remote frame if the load commits. The returned frame is only partially
  // attached to the frame tree: it has the same parent as its potential
  // replacee but is invisible to the rest of the frames in the frame tree.
  // If the load commits, call swap() to fully attach this frame.
  BLINK_EXPORT static WebLocalFrame* createProvisional(WebFrameClient*,
                                                       WebRemoteFrame*,
                                                       WebSandboxFlags);

  // Returns the WebFrame associated with the current V8 context. This
  // function can return 0 if the context is associated with a Document that
  // is not currently being displayed in a Frame.
  BLINK_EXPORT static WebLocalFrame* frameForCurrentContext();

  // Returns the frame corresponding to the given context. This can return 0
  // if the context is detached from the frame, or if the context doesn't
  // correspond to a frame (e.g., workers).
  BLINK_EXPORT static WebLocalFrame* frameForContext(v8::Local<v8::Context>);

  // Returns the frame inside a given frame or iframe element. Returns 0 if
  // the given element is not a frame, iframe or if the frame is empty.
  BLINK_EXPORT static WebLocalFrame* fromFrameOwnerElement(const WebElement&);

  // Initialization ---------------------------------------------------------

  virtual void setAutofillClient(WebAutofillClient*) = 0;
  virtual WebAutofillClient* autofillClient() = 0;
  virtual void setDevToolsAgentClient(WebDevToolsAgentClient*) = 0;
  virtual WebDevToolsAgent* devToolsAgent() = 0;

  // Hierarchy ----------------------------------------------------------

  // Get the highest-level LocalFrame in this frame's in-process subtree.
  virtual WebLocalFrame* localRoot() = 0;

  // Navigation Ping --------------------------------------------------------

  virtual void sendPings(const WebURL& destinationURL) = 0;

  // Navigation ----------------------------------------------------------

  // Runs beforeunload handlers for this frame and returns the value returned
  // by handlers.
  // Note: this may lead to the destruction of the frame.
  virtual bool dispatchBeforeUnloadEvent(bool isReload) = 0;

  // Returns a WebURLRequest corresponding to the load of the WebHistoryItem.
  virtual WebURLRequest requestFromHistoryItem(const WebHistoryItem&,
                                               WebCachePolicy) const = 0;

  // Returns a WebURLRequest corresponding to the reload of the current
  // HistoryItem.
  virtual WebURLRequest requestForReload(
      WebFrameLoadType,
      const WebURL& overrideURL = WebURL()) const = 0;

  // Load the given URL. For history navigations, a valid WebHistoryItem
  // should be given, as well as a WebHistoryLoadType.
  virtual void load(const WebURLRequest&,
                    WebFrameLoadType = WebFrameLoadType::Standard,
                    const WebHistoryItem& = WebHistoryItem(),
                    WebHistoryLoadType = WebHistoryDifferentDocumentLoad,
                    bool isClientRedirect = false) = 0;

  // Loads the given data with specific mime type and optional text
  // encoding.  For HTML data, baseURL indicates the security origin of
  // the document and is used to resolve links.  If specified,
  // unreachableURL is reported via WebDataSource::unreachableURL.  If
  // replace is false, then this data will be loaded as a normal
  // navigation.  Otherwise, the current history item will be replaced.
  virtual void loadData(const WebData&,
                        const WebString& mimeType,
                        const WebString& textEncoding,
                        const WebURL& baseURL,
                        const WebURL& unreachableURL = WebURL(),
                        bool replace = false,
                        WebFrameLoadType = WebFrameLoadType::Standard,
                        const WebHistoryItem& = WebHistoryItem(),
                        WebHistoryLoadType = WebHistoryDifferentDocumentLoad,
                        bool isClientRedirect = false) = 0;

  // On load failure, attempts to make frame's parent rendering fallback content
  // and stop this frame loading.
  virtual bool maybeRenderFallbackContent(const WebURLError&) const = 0;

  // Navigation State -------------------------------------------------------

  // Returns true if the current frame's load event has not completed.
  virtual bool isLoading() const = 0;

  // Returns true if the current frame is detaching/detached. crbug.com/654654
  virtual bool isFrameDetachedForSpecialOneOffStopTheCrashingHackBug561873()
      const = 0;

  // Returns true if there is a pending redirect or location change
  // within specified interval (in seconds). This could be caused by:
  // * an HTTP Refresh header
  // * an X-Frame-Options header
  // * the respective http-equiv meta tags
  // * window.location value being mutated
  // * CSP policy block
  // * reload
  // * form submission
  virtual bool isNavigationScheduledWithin(double intervalInSeconds) const = 0;

  // Override the normal rules for whether a load has successfully committed
  // in this frame. Used to propagate state when this frame has navigated
  // cross process.
  virtual void setCommittedFirstRealLoad() = 0;

  // Mark this frame's document as having received a user gesture, based on
  // one of its descendants having processed a user gesture.
  virtual void setHasReceivedUserGesture() = 0;

  // Orientation Changes ----------------------------------------------------

  // Notify the frame that the screen orientation has changed.
  virtual void sendOrientationChangeEvent() = 0;

  // Printing ------------------------------------------------------------

  // Returns true on success and sets the out parameter to the print preset
  // options for the document.
  virtual bool getPrintPresetOptionsForPlugin(const WebNode&,
                                              WebPrintPresetOptions*) = 0;

  // CSS3 Paged Media ----------------------------------------------------

  // Returns true if page box (margin boxes and page borders) is visible.
  virtual bool isPageBoxVisible(int pageIndex) = 0;

  // Returns true if the page style has custom size information.
  virtual bool hasCustomPageSizeStyle(int pageIndex) = 0;

  // Returns the preferred page size and margins in pixels, assuming 96
  // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
  // marginLeft must be initialized to the default values that are used if
  // auto is specified.
  virtual void pageSizeAndMarginsInPixels(int pageIndex,
                                          WebDoubleSize& pageSize,
                                          int& marginTop,
                                          int& marginRight,
                                          int& marginBottom,
                                          int& marginLeft) = 0;

  // Returns the value for a page property that is only defined when printing.
  // printBegin must have been called before this method.
  virtual WebString pageProperty(const WebString& propertyName,
                                 int pageIndex) = 0;

  // Scripting --------------------------------------------------------------
  // Executes script in the context of the current page and returns the value
  // that the script evaluated to with callback. Script execution can be
  // suspend.
  // DEPRECATED: Prefer requestExecuteScriptInIsolatedWorld().
  virtual void requestExecuteScriptAndReturnValue(
      const WebScriptSource&,
      bool userGesture,
      WebScriptExecutionCallback*) = 0;

  // Requests execution of the given function, but allowing for script
  // suspension and asynchronous execution.
  virtual void requestExecuteV8Function(v8::Local<v8::Context>,
                                        v8::Local<v8::Function>,
                                        v8::Local<v8::Value> receiver,
                                        int argc,
                                        v8::Local<v8::Value> argv[],
                                        WebScriptExecutionCallback*) = 0;

  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < EmbedderWorldIdLimit, high number used internally.
  virtual void requestExecuteScriptInIsolatedWorld(
      int worldID,
      const WebScriptSource* sourceIn,
      unsigned numSources,
      int extensionGroup,
      bool userGesture,
      WebScriptExecutionCallback*) = 0;

  // Run the task when the context of the current page is not suspended
  // otherwise run it on context resumed.
  // Method takes ownership of the passed task.
  virtual void requestRunTask(WebSuspendableTask*) const = 0;

  // Associates an isolated world with human-readable name which is useful for
  // extension debugging.
  virtual void setIsolatedWorldHumanReadableName(int worldID,
                                                 const WebString&) = 0;

  // Gets or creates the context of the isolated world
  // As in requestExecuteScriptInIsolatedWorld: 0 < worldId < EmbedderWorldIdLimit
  virtual v8::Local<v8::Context> isolatedWorldScriptContext(int worldID,
                                                            int extensionGroup) const = 0;

  // Editing -------------------------------------------------------------

  virtual void setMarkedText(const WebString& text,
                             unsigned location,
                             unsigned length) = 0;
  virtual void unmarkText() = 0;
  virtual bool hasMarkedText() const = 0;

  virtual WebRange markedRange() const = 0;

  // Returns the text range rectangle in the viepwort coordinate space.
  virtual bool firstRectForCharacterRange(unsigned location,
                                          unsigned length,
                                          WebRect&) const = 0;

  // Returns the index of a character in the Frame's text stream at the given
  // point. The point is in the viewport coordinate space. Will return
  // WTF::notFound if the point is invalid.
  virtual size_t characterIndexForPoint(const WebPoint&) const = 0;

  // Supports commands like Undo, Redo, Cut, Copy, Paste, SelectAll,
  // Unselect, etc. See EditorCommand.cpp for the full list of supported
  // commands.
  virtual bool executeCommand(const WebString&) = 0;
  virtual bool executeCommand(const WebString&, const WebString& value) = 0;
  virtual bool isCommandEnabled(const WebString&) const = 0;

  // Selection -----------------------------------------------------------

  virtual bool hasSelection() const = 0;

  virtual WebRange selectionRange() const = 0;

  virtual WebString selectionAsText() const = 0;
  virtual WebString selectionAsMarkup() const = 0;

  // Expands the selection to a word around the caret and returns
  // true. Does nothing and returns false if there is no caret or
  // there is ranged selection.
  virtual bool selectWordAroundCaret() = 0;

  // DEPRECATED: Use moveRangeSelection.
  virtual void selectRange(const WebPoint& base, const WebPoint& extent) = 0;

  virtual void selectRange(const WebRange&) = 0;
  virtual WebString rangeAsText(const WebRange&) = 0;

  // Move the current selection to the provided viewport point/points. If the
  // current selection is editable, the new selection will be restricted to
  // the root editable element.
  // |TextGranularity| represents character wrapping granularity. If
  // WordGranularity is set, WebFrame extends selection to wrap word.
  virtual void moveRangeSelection(
      const WebPoint& base,
      const WebPoint& extent,
      WebFrame::TextGranularity = CharacterGranularity) = 0;
  virtual void moveCaretSelection(const WebPoint&) = 0;

  virtual bool setEditableSelectionOffsets(int start, int end) = 0;
  virtual bool setCompositionFromExistingText(
      int compositionStart,
      int compositionEnd,
      const WebVector<WebCompositionUnderline>& underlines) = 0;
  virtual void extendSelectionAndDelete(int before, int after) = 0;

  virtual void setCaretVisible(bool) = 0;

  // Moves the selection extent point. This function does not allow the
  // selection to collapse. If the new extent is set to the same position as
  // the current base, this function will do nothing.
  virtual void moveRangeSelectionExtent(const WebPoint&) = 0;
  // Replaces the selection with the input string.
  virtual void replaceSelection(const WebString&) = 0;
  // Deletes text before and after the current cursor position, excluding the
  // selection.
  virtual void deleteSurroundingText(int before, int after) = 0;

  // Spell-checking support -------------------------------------------------
  virtual void replaceMisspelledRange(const WebString&) = 0;
  virtual void enableSpellChecking(bool) = 0;
  virtual bool isSpellCheckingEnabled() const = 0;
  virtual void removeSpellingMarkers() = 0;

  // Content Settings -------------------------------------------------------

  virtual void setContentSettingsClient(WebContentSettingsClient*) = 0;

  // Image reload -----------------------------------------------------------

  // If the provided node is an image, reload the image disabling Lo-Fi.
  virtual void reloadImage(const WebNode&) = 0;

  // Reloads all the Lo-Fi images in this WebLocalFrame. Ignores the cache and
  // reloads from the network.
  virtual void reloadLoFiImages() = 0;

  // Feature usage logging --------------------------------------------------

  virtual void didCallAddSearchProvider() = 0;
  virtual void didCallIsSearchProviderInstalled() = 0;

  // Iframe sandbox ---------------------------------------------------------

  // Returns the effective sandbox flags which are inherited from their parent
  // frame.
  virtual WebSandboxFlags effectiveSandboxFlags() const = 0;

  // Set sandbox flags that will always be forced on this frame.  This is
  // used to inherit sandbox flags from cross-process opener frames in popups.
  //
  // TODO(dcheng): Remove this once we have WebLocalFrame::createMainFrame.
  virtual void forceSandboxFlags(WebSandboxFlags) = 0;

  // Find-in-page -----------------------------------------------------------

  // Specifies the action to be taken at the end of a find-in-page session.
  enum StopFindAction {
    // No selection will be left.
    StopFindActionClearSelection,

    // The active match will remain selected.
    StopFindActionKeepSelection,

    // The active match selection will be activated.
    StopFindActionActivateSelection
  };

  // Begins a find request, which includes finding the next find match (using
  // find()) and scoping the frame for find matches if needed.
  virtual void requestFind(int identifier,
                           const WebString& searchText,
                           const WebFindOptions&) = 0;

  // Searches a frame for a given string.
  //
  // If a match is found, this function will select it (scrolling down to
  // make it visible if needed) and fill in selectionRect with the
  // location of where the match was found (in window coordinates).
  //
  // If no match is found, this function clears all tickmarks and
  // highlighting.
  //
  // Returns true if the search string was found, false otherwise.
  virtual bool find(int identifier,
                    const WebString& searchText,
                    const WebFindOptions&,
                    bool wrapWithinFrame,
                    bool* activeNow = nullptr) = 0;

  // Notifies the frame that we are no longer interested in searching.  This
  // will abort any asynchronous scoping effort already under way (see the
  // function TextFinder::scopeStringMatches for details) and erase all
  // tick-marks and highlighting from the previous search.  It will also
  // follow the specified StopFindAction.
  virtual void stopFinding(StopFindAction) = 0;

  // This function is called during the scoping effort to keep a running tally
  // of the accumulated total match-count in the frame.  After updating the
  // count it will notify the WebViewClient about the new count.
  virtual void increaseMatchCount(int count, int identifier) = 0;

  // Returns a counter that is incremented when the find-in-page markers are
  // changed on the frame. Switching the active marker doesn't change the
  // current version.
  virtual int findMatchMarkersVersion() const = 0;

  // Returns the bounding box of the active find-in-page match marker or an
  // empty rect if no such marker exists. The rect is returned in find-in-page
  // coordinates.
  virtual WebFloatRect activeFindMatchRect() = 0;

  // Swaps the contents of the provided vector with the bounding boxes of the
  // find-in-page match markers from the frame. The bounding boxes are
  // returned in find-in-page coordinates.
  virtual void findMatchRects(WebVector<WebFloatRect>&) = 0;

  // Selects the find-in-page match closest to the provided point in
  // find-in-page coordinates. Returns the ordinal of such match or -1 if none
  // could be found. If not null, selectionRect is set to the bounding box of
  // the selected match in window coordinates.
  virtual int selectNearestFindMatch(const WebFloatPoint&,
                                     WebRect* selectionRect) = 0;

  // Returns the distance (squared) to the closest find-in-page match from the
  // provided point, in find-in-page coordinates.
  virtual float distanceToNearestFindMatch(const WebFloatPoint&) = 0;

  // Set the tickmarks for the frame. This will override the default tickmarks
  // generated by find results. If this is called with an empty array, the
  // default behavior will be restored.
  virtual void setTickmarks(const WebVector<WebRect>&) = 0;

  // Clears the active find match in the frame, if one exists.
  virtual void clearActiveFindMatch() = 0;

  // Context menu -----------------------------------------------------------

  // Returns the node that the context menu opened over.
  virtual WebNode contextMenuNode() const = 0;

  // Returns the WebFrameWidget associated with this frame if there is one or
  // nullptr otherwise.
  virtual WebFrameWidget* frameWidget() const = 0;

  // Copy to the clipboard the image located at a particular point in visual
  // viewport coordinates.
  virtual void copyImageAt(const WebPoint&) = 0;

  // Save as the image located at a particular point in visual viewport
  // coordinates.
  virtual void saveImageAt(const WebPoint&) = 0;

  // TEMP: Usage count for chrome.loadtimes deprecation.
  // This will be removed following the deprecation.
  virtual void usageCountChromeLoadTimes(const WebString& metric) = 0;

 protected:
  explicit WebLocalFrame(WebTreeScopeType scope) : WebFrame(scope) {}

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebLocalFrame.
  bool isWebLocalFrame() const override = 0;
  WebLocalFrame* toWebLocalFrame() override = 0;
  bool isWebRemoteFrame() const override = 0;
  WebRemoteFrame* toWebRemoteFrame() override = 0;
};

}  // namespace blink

#endif  // WebLocalFrame_h
