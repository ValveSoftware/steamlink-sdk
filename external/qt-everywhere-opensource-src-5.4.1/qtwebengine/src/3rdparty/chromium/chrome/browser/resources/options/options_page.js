// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var FocusOutlineManager = cr.ui.FocusOutlineManager;

  /////////////////////////////////////////////////////////////////////////////
  // OptionsPage class:

  /**
   * Base class for options page.
   * @constructor
   * @param {string} name Options page name.
   * @param {string} title Options page title, used for history.
   * @extends {EventTarget}
   */
  function OptionsPage(name, title, pageDivName) {
    this.name = name;
    this.title = title;
    this.pageDivName = pageDivName;
    this.pageDiv = $(this.pageDivName);
    // |pageDiv.page| is set to the page object (this) when the page is visible
    // to track which page is being shown when multiple pages can share the same
    // underlying div.
    this.pageDiv.page = null;
    this.tab = null;
    this.lastFocusedElement = null;
  }

  /**
   * This is the absolute difference maintained between standard and
   * fixed-width font sizes. Refer http://crbug.com/91922.
   * @const
   */
  OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD = 3;

  /**
   * Offset of page container in pixels, to allow room for side menu.
   * Simplified settings pages can override this if they don't use the menu.
   * The default (155) comes from -webkit-margin-start in uber_shared.css
   * @private
   */
  OptionsPage.horizontalOffset = 155;

  /**
   * Main level option pages. Maps lower-case page names to the respective page
   * object.
   * @protected
   */
  OptionsPage.registeredPages = {};

  /**
   * Pages which are meant to behave like modal dialogs. Maps lower-case overlay
   * names to the respective overlay object.
   * @protected
   */
  OptionsPage.registeredOverlayPages = {};

  /**
   * True if options page is served from a dialog.
   */
  OptionsPage.isDialog = false;

  /**
   * Gets the default page (to be shown on initial load).
   */
  OptionsPage.getDefaultPage = function() {
    return BrowserOptions.getInstance();
  };

  /**
   * Shows the default page.
   */
  OptionsPage.showDefaultPage = function() {
    this.navigateToPage(this.getDefaultPage().name);
  };

  /**
   * "Navigates" to a page, meaning that the page will be shown and the
   * appropriate entry is placed in the history.
   * @param {string} pageName Page name.
   */
  OptionsPage.navigateToPage = function(pageName) {
    this.showPageByName(pageName, true);
  };

  /**
   * Shows a registered page. This handles both top-level and overlay pages.
   * @param {string} pageName Page name.
   * @param {boolean} updateHistory True if we should update the history after
   *     showing the page.
   * @param {Object=} opt_propertyBag An optional bag of properties including
   *     replaceState (if history state should be replaced instead of pushed).
   * @private
   */
  OptionsPage.showPageByName = function(pageName,
                                        updateHistory,
                                        opt_propertyBag) {
    // If |opt_propertyBag| is non-truthy, homogenize to object.
    opt_propertyBag = opt_propertyBag || {};

    // If a bubble is currently being shown, hide it.
    this.hideBubble();

    // Find the currently visible root-level page.
    var rootPage = null;
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (page.visible && !page.parentPage) {
        rootPage = page;
        break;
      }
    }

    // Find the target page.
    var targetPage = this.registeredPages[pageName.toLowerCase()];
    if (!targetPage || !targetPage.canShowPage()) {
      // If it's not a page, try it as an overlay.
      if (!targetPage && this.showOverlay_(pageName, rootPage)) {
        if (updateHistory)
          this.updateHistoryState_(!!opt_propertyBag.replaceState);
        this.updateTitle_();
        return;
      } else {
        targetPage = this.getDefaultPage();
      }
    }

    pageName = targetPage.name.toLowerCase();
    var targetPageWasVisible = targetPage.visible;

    // Determine if the root page is 'sticky', meaning that it
    // shouldn't change when showing an overlay. This can happen for special
    // pages like Search.
    var isRootPageLocked =
        rootPage && rootPage.sticky && targetPage.parentPage;

    var allPageNames = Array.prototype.concat.call(
        Object.keys(this.registeredPages),
        Object.keys(this.registeredOverlayPages));

    // Notify pages if they will be hidden.
    for (var i = 0; i < allPageNames.length; ++i) {
      var name = allPageNames[i];
      var page = this.registeredPages[name] ||
                 this.registeredOverlayPages[name];
      if (!page.parentPage && isRootPageLocked)
        continue;
      if (page.willHidePage && name != pageName &&
          !page.isAncestorOfPage(targetPage)) {
        page.willHidePage();
      }
    }

    // Update visibilities to show only the hierarchy of the target page.
    for (var i = 0; i < allPageNames.length; ++i) {
      var name = allPageNames[i];
      var page = this.registeredPages[name] ||
                 this.registeredOverlayPages[name];
      if (!page.parentPage && isRootPageLocked)
        continue;
      page.visible = name == pageName || page.isAncestorOfPage(targetPage);
    }

    // Update the history and current location.
    if (updateHistory)
      this.updateHistoryState_(!!opt_propertyBag.replaceState);

    // Update focus if any other control was focused on the previous page,
    // or the previous page is not known.
    if (document.activeElement != document.body &&
        (!rootPage || rootPage.pageDiv.contains(document.activeElement))) {
      targetPage.focus();
    }

    // Notify pages if they were shown.
    for (var i = 0; i < allPageNames.length; ++i) {
      var name = allPageNames[i];
      var page = this.registeredPages[name] ||
                 this.registeredOverlayPages[name];
      if (!page.parentPage && isRootPageLocked)
        continue;
      if (!targetPageWasVisible && page.didShowPage &&
          (name == pageName || page.isAncestorOfPage(targetPage))) {
        page.didShowPage();
      }
    }

    // Update the document title. Do this after didShowPage was called, in case
    // a page decides to change its title.
    this.updateTitle_();
  };

  /**
   * Scrolls the page to the correct position (the top when opening an overlay,
   * or the old scroll position a previously hidden overlay becomes visible).
   * @private
   */
  OptionsPage.updateScrollPosition_ = function() {
    var container = $('page-container');
    var scrollTop = container.oldScrollTop || 0;
    container.oldScrollTop = undefined;
    window.scroll(scrollLeftForDocument(document), scrollTop);
  };

  /**
   * Updates the title to title of the current page.
   * @private
   */
  OptionsPage.updateTitle_ = function() {
    var page = this.getTopmostVisiblePage();
    uber.setTitle(page.title);
  };

  /**
   * Pushes the current page onto the history stack, replacing the current entry
   * if appropriate.
   * @param {boolean} replace If true, allow no history events to be created.
   * @param {object=} opt_params A bag of optional params, including:
   *     {boolean} ignoreHash Whether to include the hash or not.
   * @private
   */
  OptionsPage.updateHistoryState_ = function(replace, opt_params) {
    if (OptionsPage.isDialog)
      return;

    var page = this.getTopmostVisiblePage();
    var path = window.location.pathname + window.location.hash;
    if (path)
      path = path.slice(1).replace(/\/(?:#|$)/, '');  // Remove trailing slash.

    // If the page is already in history (the user may have clicked the same
    // link twice, or this is the initial load), do nothing.
    var hash = opt_params && opt_params.ignoreHash ? '' : window.location.hash;
    var newPath = (page == this.getDefaultPage() ? '' : page.name) + hash;
    if (path == newPath)
      return;

    var historyFunction = replace ? uber.replaceState : uber.pushState;
    historyFunction.call(uber, {}, newPath);
  };

  /**
   * Shows a registered Overlay page. Does not update history.
   * @param {string} overlayName Page name.
   * @param {OptionPage} rootPage The currently visible root-level page.
   * @return {boolean} whether we showed an overlay.
   */
  OptionsPage.showOverlay_ = function(overlayName, rootPage) {
    var overlay = this.registeredOverlayPages[overlayName.toLowerCase()];
    if (!overlay || !overlay.canShowPage())
      return false;

    // Save the currently focused element in the page for restoration later.
    var currentPage = this.getTopmostVisiblePage();
    if (currentPage)
      currentPage.lastFocusedElement = document.activeElement;

    if ((!rootPage || !rootPage.sticky) &&
        overlay.parentPage &&
        !overlay.parentPage.visible) {
      this.showPageByName(overlay.parentPage.name, false);
    }

    if (!overlay.visible) {
      overlay.visible = true;
      if (overlay.didShowPage) overlay.didShowPage();
    }

    // Change focus to the overlay if any other control was focused by keyboard
    // before. Otherwise, no one should have focus.
    if (document.activeElement != document.body) {
      if (FocusOutlineManager.forDocument(document).visible) {
        overlay.focus();
      } else if (!overlay.pageDiv.contains(document.activeElement)) {
        document.activeElement.blur();
      }
    }

    if ($('search-field') && $('search-field').value == '') {
      var section = overlay.associatedSection;
      if (section)
        options.BrowserOptions.scrollToSection(section);
    }

    return true;
  };

  /**
   * Returns whether or not an overlay is visible.
   * @return {boolean} True if an overlay is visible.
   * @private
   */
  OptionsPage.isOverlayVisible_ = function() {
    return this.getVisibleOverlay_() != null;
  };

  /**
   * Returns the currently visible overlay, or null if no page is visible.
   * @return {OptionPage} The visible overlay.
   */
  OptionsPage.getVisibleOverlay_ = function() {
    var topmostPage = null;
    for (var name in this.registeredOverlayPages) {
      var page = this.registeredOverlayPages[name];
      if (page.visible &&
          (!topmostPage || page.nestingLevel > topmostPage.nestingLevel)) {
        topmostPage = page;
      }
    }
    return topmostPage;
  };

  /**
   * Restores the last focused element on a given page.
   */
  OptionsPage.restoreLastFocusedElement_ = function() {
    var currentPage = this.getTopmostVisiblePage();
    if (currentPage.lastFocusedElement)
      currentPage.lastFocusedElement.focus();
  };

  /**
   * Closes the visible overlay. Updates the history state after closing the
   * overlay.
   */
  OptionsPage.closeOverlay = function() {
    var overlay = this.getVisibleOverlay_();
    if (!overlay)
      return;

    overlay.visible = false;

    if (overlay.didClosePage) overlay.didClosePage();
    this.updateHistoryState_(false, {ignoreHash: true});
    this.updateTitle_();

    this.restoreLastFocusedElement_();
  };

  /**
   * Closes all overlays and updates the history after each closed overlay.
   */
  OptionsPage.closeAllOverlays = function() {
    while (this.isOverlayVisible_()) {
      this.closeOverlay();
    }
  };

  /**
   * Cancels (closes) the overlay, due to the user pressing <Esc>.
   */
  OptionsPage.cancelOverlay = function() {
    // Blur the active element to ensure any changed pref value is saved.
    document.activeElement.blur();
    var overlay = this.getVisibleOverlay_();
    // Let the overlay handle the <Esc> if it wants to.
    if (overlay.handleCancel) {
      overlay.handleCancel();
      this.restoreLastFocusedElement_();
    } else {
      this.closeOverlay();
    }
  };

  /**
   * Hides the visible overlay. Does not affect the history state.
   * @private
   */
  OptionsPage.hideOverlay_ = function() {
    var overlay = this.getVisibleOverlay_();
    if (overlay)
      overlay.visible = false;
  };

  /**
   * Returns the pages which are currently visible, ordered by nesting level
   * (ascending).
   * @return {Array.OptionPage} The pages which are currently visible, ordered
   * by nesting level (ascending).
   */
  OptionsPage.getVisiblePages_ = function() {
    var visiblePages = [];
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (page.visible)
        visiblePages[page.nestingLevel] = page;
    }
    return visiblePages;
  };

  /**
   * Returns the topmost visible page (overlays excluded).
   * @return {OptionPage} The topmost visible page aside any overlay.
   * @private
   */
  OptionsPage.getTopmostVisibleNonOverlayPage_ = function() {
    var topPage = null;
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (page.visible &&
          (!topPage || page.nestingLevel > topPage.nestingLevel))
        topPage = page;
    }

    return topPage;
  };

  /**
   * Returns the topmost visible page, or null if no page is visible.
   * @return {OptionPage} The topmost visible page.
   */
  OptionsPage.getTopmostVisiblePage = function() {
    // Check overlays first since they're top-most if visible.
    return this.getVisibleOverlay_() || this.getTopmostVisibleNonOverlayPage_();
  };

  /**
   * Returns the currently visible bubble, or null if no bubble is visible.
   * @return {AutoCloseBubble} The bubble currently being shown.
   */
  OptionsPage.getVisibleBubble = function() {
    var bubble = OptionsPage.bubble_;
    return bubble && !bubble.hidden ? bubble : null;
  };

  /**
   * Shows an informational bubble displaying |content| and pointing at the
   * |target| element. If |content| has focusable elements, they join the
   * current page's tab order as siblings of |domSibling|.
   * @param {HTMLDivElement} content The content of the bubble.
   * @param {HTMLElement} target The element at which the bubble points.
   * @param {HTMLElement} domSibling The element after which the bubble is added
   *                      to the DOM.
   * @param {cr.ui.ArrowLocation} location The arrow location.
   */
  OptionsPage.showBubble = function(content, target, domSibling, location) {
    OptionsPage.hideBubble();

    var bubble = new cr.ui.AutoCloseBubble;
    bubble.anchorNode = target;
    bubble.domSibling = domSibling;
    bubble.arrowLocation = location;
    bubble.content = content;
    bubble.show();
    OptionsPage.bubble_ = bubble;
  };

  /**
   * Hides the currently visible bubble, if any.
   */
  OptionsPage.hideBubble = function() {
    if (OptionsPage.bubble_)
      OptionsPage.bubble_.hide();
  };

  /**
   * Shows the tab contents for the given navigation tab.
   * @param {!Element} tab The tab that the user clicked.
   */
  OptionsPage.showTab = function(tab) {
    // Search parents until we find a tab, or the nav bar itself. This allows
    // tabs to have child nodes, e.g. labels in separately-styled spans.
    while (tab && !tab.classList.contains('subpages-nav-tabs') &&
           !tab.classList.contains('tab')) {
      tab = tab.parentNode;
    }
    if (!tab || !tab.classList.contains('tab'))
      return;

    // Find tab bar of the tab.
    var tabBar = tab;
    while (tabBar && !tabBar.classList.contains('subpages-nav-tabs')) {
      tabBar = tabBar.parentNode;
    }
    if (!tabBar)
      return;

    if (tabBar.activeNavTab != null) {
      tabBar.activeNavTab.classList.remove('active-tab');
      $(tabBar.activeNavTab.getAttribute('tab-contents')).classList.
          remove('active-tab-contents');
    }

    tab.classList.add('active-tab');
    $(tab.getAttribute('tab-contents')).classList.add('active-tab-contents');
    tabBar.activeNavTab = tab;
  };

  /**
   * Registers new options page.
   * @param {OptionsPage} page Page to register.
   */
  OptionsPage.register = function(page) {
    this.registeredPages[page.name.toLowerCase()] = page;
    page.initializePage();
  };

  /**
   * Find an enclosing section for an element if it exists.
   * @param {Element} element Element to search.
   * @return {OptionPage} The section element, or null.
   * @private
   */
  OptionsPage.findSectionForNode_ = function(node) {
    while (node = node.parentNode) {
      if (node.nodeName == 'SECTION')
        return node;
    }
    return null;
  };

  /**
   * Registers a new Overlay page.
   * @param {OptionsPage} overlay Overlay to register.
   * @param {OptionsPage} parentPage Associated parent page for this overlay.
   * @param {Array} associatedControls Array of control elements associated with
   *   this page.
   */
  OptionsPage.registerOverlay = function(overlay,
                                         parentPage,
                                         associatedControls) {
    this.registeredOverlayPages[overlay.name.toLowerCase()] = overlay;
    overlay.parentPage = parentPage;
    if (associatedControls) {
      overlay.associatedControls = associatedControls;
      if (associatedControls.length) {
        overlay.associatedSection =
            this.findSectionForNode_(associatedControls[0]);
      }

      // Sanity check.
      for (var i = 0; i < associatedControls.length; ++i) {
        assert(associatedControls[i], 'Invalid element passed.');
      }
    }

    // Reverse the button strip for Windows and CrOS. See the documentation of
    // reverseButtonStripIfNecessary_() for an explanation of why this is done.
    if (cr.isWindows || cr.isChromeOS)
      this.reverseButtonStripIfNecessary_(overlay);

    overlay.tab = undefined;
    overlay.isOverlay = true;
    overlay.initializePage();
  };

  /**
   * Reverses the child elements of a button strip if it hasn't already been
   * reversed. This is necessary because WebKit does not alter the tab order for
   * elements that are visually reversed using -webkit-box-direction: reverse,
   * and the button order is reversed for views. See http://webk.it/62664 for
   * more information.
   * @param {Object} overlay The overlay containing the button strip to reverse.
   * @private
   */
  OptionsPage.reverseButtonStripIfNecessary_ = function(overlay) {
    var buttonStrips =
        overlay.pageDiv.querySelectorAll('.button-strip:not([reversed])');

    // Reverse all button-strips in the overlay.
    for (var j = 0; j < buttonStrips.length; j++) {
      var buttonStrip = buttonStrips[j];

      var childNodes = buttonStrip.childNodes;
      for (var i = childNodes.length - 1; i >= 0; i--)
        buttonStrip.appendChild(childNodes[i]);

      buttonStrip.setAttribute('reversed', '');
    }
  };

  /**
   * Returns the name of the page from the current path.
   */
  OptionsPage.getPageNameFromPath = function() {
    var path = location.pathname;
    if (path.length <= 1)
      return this.getDefaultPage().name;

    // Skip starting slash and remove trailing slash (if any).
    return path.slice(1).replace(/\/$/, '');
  };

  /**
   * Callback for window.onpopstate to handle back/forward navigations.
   * @param {string} pageName The current page name.
   * @param {Object} data State data pushed into history.
   */
  OptionsPage.setState = function(pageName, data) {
    var currentOverlay = this.getVisibleOverlay_();
    var lowercaseName = pageName.toLowerCase();
    var newPage = this.registeredPages[lowercaseName] ||
                  this.registeredOverlayPages[lowercaseName] ||
                  this.getDefaultPage();
    if (currentOverlay && !currentOverlay.isAncestorOfPage(newPage)) {
      currentOverlay.visible = false;
      if (currentOverlay.didClosePage) currentOverlay.didClosePage();
    }
    this.showPageByName(pageName, false);
  };

  /**
   * Callback for window.onbeforeunload. Used to notify overlays that they will
   * be closed.
   */
  OptionsPage.willClose = function() {
    var overlay = this.getVisibleOverlay_();
    if (overlay && overlay.didClosePage)
      overlay.didClosePage();
  };

  /**
   * Freezes/unfreezes the scroll position of the root page container.
   * @param {boolean} freeze Whether the page should be frozen.
   * @private
   */
  OptionsPage.setRootPageFrozen_ = function(freeze) {
    var container = $('page-container');
    if (container.classList.contains('frozen') == freeze)
      return;

    if (freeze) {
      // Lock the width, since auto width computation may change.
      container.style.width = window.getComputedStyle(container).width;
      container.oldScrollTop = scrollTopForDocument(document);
      container.classList.add('frozen');
      var verticalPosition =
          container.getBoundingClientRect().top - container.oldScrollTop;
      container.style.top = verticalPosition + 'px';
      this.updateFrozenElementHorizontalPosition_(container);
    } else {
      container.classList.remove('frozen');
      container.style.top = '';
      container.style.left = '';
      container.style.right = '';
      container.style.width = '';
    }
  };

  /**
   * Freezes/unfreezes the scroll position of the root page based on the current
   * page stack.
   */
  OptionsPage.updateRootPageFreezeState = function() {
    var topPage = OptionsPage.getTopmostVisiblePage();
    if (topPage)
      this.setRootPageFrozen_(topPage.isOverlay);
  };

  /**
   * Initializes the complete options page.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  OptionsPage.initialize = function() {
    chrome.send('coreOptionsInitialize');
    uber.onContentFrameLoaded();
    FocusOutlineManager.forDocument(document);
    document.addEventListener('scroll', this.handleScroll_.bind(this));

    // Trigger the scroll handler manually to set the initial state.
    this.handleScroll_();

    // Shake the dialog if the user clicks outside the dialog bounds.
    var containers = [$('overlay-container-1'), $('overlay-container-2')];
    for (var i = 0; i < containers.length; i++) {
      var overlay = containers[i];
      cr.ui.overlay.setupOverlay(overlay);
      overlay.addEventListener('cancelOverlay',
                               OptionsPage.cancelOverlay.bind(OptionsPage));
    }

    cr.ui.overlay.globalInitialization();
  };

  /**
   * Does a bounds check for the element on the given x, y client coordinates.
   * @param {Element} e The DOM element.
   * @param {number} x The client X to check.
   * @param {number} y The client Y to check.
   * @return {boolean} True if the point falls within the element's bounds.
   * @private
   */
  OptionsPage.elementContainsPoint_ = function(e, x, y) {
    var clientRect = e.getBoundingClientRect();
    return x >= clientRect.left && x <= clientRect.right &&
        y >= clientRect.top && y <= clientRect.bottom;
  };

  /**
   * Called when the page is scrolled; moves elements that are position:fixed
   * but should only behave as if they are fixed for vertical scrolling.
   * @private
   */
  OptionsPage.handleScroll_ = function() {
    this.updateAllFrozenElementPositions_();
  };

  /**
   * Updates all frozen pages to match the horizontal scroll position.
   * @private
   */
  OptionsPage.updateAllFrozenElementPositions_ = function() {
    var frozenElements = document.querySelectorAll('.frozen');
    for (var i = 0; i < frozenElements.length; i++)
      this.updateFrozenElementHorizontalPosition_(frozenElements[i]);
  };

  /**
   * Updates the given frozen element to match the horizontal scroll position.
   * @param {HTMLElement} e The frozen element to update.
   * @private
   */
  OptionsPage.updateFrozenElementHorizontalPosition_ = function(e) {
    if (isRTL()) {
      e.style.right = OptionsPage.horizontalOffset + 'px';
    } else {
      var scrollLeft = scrollLeftForDocument(document);
      e.style.left = OptionsPage.horizontalOffset - scrollLeft + 'px';
    }
  };

  /**
   * Change the horizontal offset used to reposition elements while showing an
   * overlay from the default.
   */
  OptionsPage.setHorizontalOffset = function(value) {
    OptionsPage.horizontalOffset = value;
  };

  OptionsPage.setClearPluginLSODataEnabled = function(enabled) {
    if (enabled) {
      document.documentElement.setAttribute(
          'flashPluginSupportsClearSiteData', '');
    } else {
      document.documentElement.removeAttribute(
          'flashPluginSupportsClearSiteData');
    }
    if (navigator.plugins['Shockwave Flash'])
      document.documentElement.setAttribute('hasFlashPlugin', '');
  };

  OptionsPage.setPepperFlashSettingsEnabled = function(enabled) {
    if (enabled) {
      document.documentElement.setAttribute(
          'enablePepperFlashSettings', '');
    } else {
      document.documentElement.removeAttribute(
          'enablePepperFlashSettings');
    }
  };

  OptionsPage.setIsSettingsApp = function() {
    document.documentElement.classList.add('settings-app');
  };

  OptionsPage.isSettingsApp = function() {
    return document.documentElement.classList.contains('settings-app');
  };

  /**
   * Whether the page is still loading (i.e. onload hasn't finished running).
   * @return {boolean} Whether the page is still loading.
   */
  OptionsPage.isLoading = function() {
    return document.documentElement.classList.contains('loading');
  };

  OptionsPage.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * The parent page of this option page, or null for top-level pages.
     * @type {OptionsPage}
     */
    parentPage: null,

    /**
     * The section on the parent page that is associated with this page.
     * Can be null.
     * @type {Element}
     */
    associatedSection: null,

    /**
     * An array of controls that are associated with this page.  The first
     * control should be located on a top-level page.
     * @type {OptionsPage}
     */
    associatedControls: null,

    /**
     * Initializes page content.
     */
    initializePage: function() {},

    /**
     * Sets focus on the first focusable element. Override for a custom focus
     * strategy.
     */
    focus: function() {
      // Do not change focus if any control on this page is already focused.
      if (this.pageDiv.contains(document.activeElement))
        return;

      var elements = this.pageDiv.querySelectorAll(
          'input, list, select, textarea, button');
      for (var i = 0; i < elements.length; i++) {
        var element = elements[i];
        // Try to focus. If fails, then continue.
        element.focus();
        if (document.activeElement == element)
          return;
      }
    },

    /**
     * Gets the container div for this page if it is an overlay.
     * @type {HTMLElement}
     */
    get container() {
      assert(this.isOverlay);
      return this.pageDiv.parentNode;
    },

    /**
     * Gets page visibility state.
     * @type {boolean}
     */
    get visible() {
      // If this is an overlay dialog it is no longer considered visible while
      // the overlay is fading out. See http://crbug.com/118629.
      if (this.isOverlay &&
          this.container.classList.contains('transparent')) {
        return false;
      }
      if (this.pageDiv.hidden)
        return false;
      return this.pageDiv.page == this;
    },

    /**
     * Sets page visibility.
     * @type {boolean}
     */
    set visible(visible) {
      if ((this.visible && visible) || (!this.visible && !visible))
        return;

      // If using an overlay, the visibility of the dialog is toggled at the
      // same time as the overlay to show the dialog's out transition. This
      // is handled in setOverlayVisible.
      if (this.isOverlay) {
        this.setOverlayVisible_(visible);
      } else {
        this.pageDiv.page = this;
        this.pageDiv.hidden = !visible;
        this.onVisibilityChanged_();
      }

      cr.dispatchPropertyChange(this, 'visible', visible, !visible);
    },

    /**
     * Shows or hides an overlay (including any visible dialog).
     * @param {boolean} visible Whether the overlay should be visible or not.
     * @private
     */
    setOverlayVisible_: function(visible) {
      assert(this.isOverlay);
      var pageDiv = this.pageDiv;
      var container = this.container;

      if (visible)
        uber.invokeMethodOnParent('beginInterceptingEvents');

      if (container.hidden != visible) {
        if (visible) {
          // If the container is set hidden and then immediately set visible
          // again, the fadeCompleted_ callback would cause it to be erroneously
          // hidden again. Removing the transparent tag avoids that.
          container.classList.remove('transparent');

          // Hide all dialogs in this container since a different one may have
          // been previously visible before fading out.
          var pages = container.querySelectorAll('.page');
          for (var i = 0; i < pages.length; i++)
            pages[i].hidden = true;
          // Show the new dialog.
          pageDiv.hidden = false;
          pageDiv.page = this;
        }
        return;
      }

      var self = this;
      var loading = OptionsPage.isLoading();
      if (!loading) {
        // TODO(flackr): Use an event delegate to avoid having to subscribe and
        // unsubscribe for webkitTransitionEnd events.
        container.addEventListener('webkitTransitionEnd', function f(e) {
            var propName = e.propertyName;
            if (e.target != e.currentTarget ||
                (propName && propName != 'opacity')) {
              return;
            }
            container.removeEventListener('webkitTransitionEnd', f);
            self.fadeCompleted_();
        });
        // -webkit-transition is 200ms. Let's wait for 400ms.
        ensureTransitionEndEvent(container, 400);
      }

      if (visible) {
        container.hidden = false;
        pageDiv.hidden = false;
        pageDiv.page = this;
        // NOTE: This is a hacky way to force the container to layout which
        // will allow us to trigger the webkit transition.
        container.scrollTop;

        this.pageDiv.removeAttribute('aria-hidden');
        if (this.parentPage) {
          this.parentPage.pageDiv.parentElement.setAttribute('aria-hidden',
                                                             true);
        }
        container.classList.remove('transparent');
        this.onVisibilityChanged_();
      } else {
        // Kick change events for text fields.
        if (pageDiv.contains(document.activeElement))
          document.activeElement.blur();
        container.classList.add('transparent');
      }

      if (loading)
        this.fadeCompleted_();
    },

    /**
     * Called when a container opacity transition finishes.
     * @private
     */
    fadeCompleted_: function() {
      if (this.container.classList.contains('transparent')) {
        this.pageDiv.hidden = true;
        this.container.hidden = true;

        if (this.parentPage)
          this.parentPage.pageDiv.parentElement.removeAttribute('aria-hidden');

        if (this.nestingLevel == 1)
          uber.invokeMethodOnParent('stopInterceptingEvents');

        this.onVisibilityChanged_();
      }
    },

    /**
     * Called when a page is shown or hidden to update the root options page
     * based on this page's visibility.
     * @private
     */
    onVisibilityChanged_: function() {
      OptionsPage.updateRootPageFreezeState();

      if (this.isOverlay && !this.visible)
        OptionsPage.updateScrollPosition_();
    },

    /**
     * The nesting level of this page.
     * @type {number} The nesting level of this page (0 for top-level page)
     */
    get nestingLevel() {
      var level = 0;
      var parent = this.parentPage;
      while (parent) {
        level++;
        parent = parent.parentPage;
      }
      return level;
    },

    /**
     * Whether the page is considered 'sticky', such that it will
     * remain a top-level page even if sub-pages change.
     * @type {boolean} True if this page is sticky.
     */
    get sticky() {
      return false;
    },

    /**
     * Checks whether this page is an ancestor of the given page in terms of
     * subpage nesting.
     * @param {OptionsPage} page The potential descendent of this page.
     * @return {boolean} True if |page| is nested under this page.
     */
    isAncestorOfPage: function(page) {
      var parent = page.parentPage;
      while (parent) {
        if (parent == this)
          return true;
        parent = parent.parentPage;
      }
      return false;
    },

    /**
     * Whether it should be possible to show the page.
     * @return {boolean} True if the page should be shown.
     */
    canShowPage: function() {
      return true;
    },
  };

  // Export
  return {
    OptionsPage: OptionsPage
  };
});
