// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper base class for all help pages and overlays, which controls
// overlays, focus and scroll. This class is partially based on
// OptionsPage, but simpler and contains only overlay- and focus-
// handling logic. As in OptionsPage each page can be an overlay itself,
// but each page contains its own list of registered overlays which can be
// displayed over it.
//
// TODO (ygorshenin@): crbug.com/313244.
cr.define('help', function() {
  function HelpBasePage() {
  }

  HelpBasePage.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * name of the page, should be the same as an id of the
     * corresponding HTMLDivElement.
     */
    name: null,

    /**
     * HTML counterpart of this page.
     */
    pageDiv: null,

    /**
     * True if current page is overlay.
     */
    isOverlay: false,

    /**
     * HTMLElement that was last focused when this page was the
     * topmost.
     */
    lastFocusedElement: null,

    /**
     * State of vertical scrollbars when this page was the topmost.
     */
    lastScrollTop: 0,

    /**
     * Dictionary of registered overlays.
     */
    registeredOverlays: {},

    /**
     * Stores currently focused element.
     * @private
     */
    storeLastFocusedElement_: function() {
      if (this.pageDiv.contains(document.activeElement))
        this.lastFocusedElement = document.activeElement;
    },

    /**
     * Restores focus to the last focused element on this page.
     * @private
     */
    restoreLastFocusedElement_: function() {
      if (this.lastFocusedElement)
        this.lastFocusedElement.focus();
      else
        this.focus();
    },

    /**
     * Shows or hides current page iff it's an overlay.
     * @param {boolean} visible True if overlay should be displayed.
     * @private
     */
    setOverlayVisible_: function(visible) {
      assert(this.isOverlay);
      this.container.hidden = !visible;
      if (visible)
        this.pageDiv.classList.add('showing');
      else
        this.pageDiv.classList.remove('showing');
    },

    /**
     * @return {HTMLDivElement}  visible non-overlay page or
     * null, if there are no visible non-overlay pages.
     * @private
     */
    getVisibleNonOverlay_: function() {
      if (this.isOverlay || !this.visible)
        return null;
      return this;
    },

    /**
     * @return {HTMLDivElement} Visible overlay page, or null,
     * if there are no visible overlay pages.
     * @private
     */
    getVisibleOverlay_: function() {
      for (var name in this.registeredOverlays) {
        var overlay = this.registeredOverlays[name];
        if (overlay.visible)
          return overlay;
      }
      return null;
    },

    /**
     * Freezes current page, makes it impossible to scroll it.
     * @param {boolean} freeze True if the page should be frozen.
     * @private
     */
    freeze_: function(freeze) {
      var scrollLeft = scrollLeftForDocument(document);
      if (freeze) {
        this.lastScrollTop = scrollTopForDocument(document);
        document.body.style.overflow = 'hidden';
        window.scroll(scrollLeft, 0);
      } else {
        document.body.style.overflow = 'auto';
        window.scroll(scrollLeft, this.lastScrollTop);
      }
    },

    /**
     * Initializes current page.
     * @param {string} name Name of the current page.
     */
    initialize: function(name) {
      this.name = name;
      this.pageDiv = $(name);
    },

    /**
     * Called before overlay is displayed.
     */
    onBeforeShow: function() {
    },

    /**
     * @return {HTMLDivElement} Topmost visible page, or null, if
     * there are no visible pages.
     */
    getTopmostVisiblePage: function() {
      return this.getVisibleOverlay_() || this.getVisibleNonOverlay_();
    },

    /**
     * Registers overlay.
     * @param {HelpBasePage} overlay Overlay that should be registered.
     */
    registerOverlay: function(overlay) {
      this.registeredOverlays[overlay.name] = overlay;
      overlay.isOverlay = true;
    },

    /**
     * Shows or hides current page.
     * @param {boolean} visible True if current page should be displayed.
     */
    set visible(visible) {
      if (this.visible == visible)
        return;

      if (!visible)
        this.storeLastFocusedElement_();

      if (this.isOverlay)
        this.setOverlayVisible_(visible);
      else
        this.pageDiv.hidden = !visible;

      if (visible)
        this.restoreLastFocusedElement_();

      if (visible)
        this.onBeforeShow();
    },

    /**
     * Returns true if current page is visible.
     * @return {boolean} True if current page is visible.
     */
    get visible() {
      if (this.isOverlay)
        return this.pageDiv.classList.contains('showing');
      return !this.pageDiv.hidden;
    },

    /**
     * This method returns overlay container, it should be called only
     * on overlays.
     * @return {HTMLDivElement} overlay container.
     */
    get container() {
      assert(this.isOverlay);
      return this.pageDiv.parentNode;
    },

    /**
     * Shows registered overlay.
     * @param {string} name Name of registered overlay to show.
     */
    showOverlay: function(name) {
      var currentPage = this.getTopmostVisiblePage();
      currentPage.storeLastFocusedElement_();
      currentPage.freeze_(true);

      var overlay = this.registeredOverlays[name];
      if (!overlay)
        return;
      overlay.visible = true;
    },

    /**
     * Hides currently displayed overlay.
     */
    closeOverlay: function() {
      var overlay = this.getVisibleOverlay_();
      if (!overlay)
        return;
      overlay.visible = false;

      var currentPage = this.getTopmostVisiblePage();
      currentPage.restoreLastFocusedElement_();
      currentPage.freeze_(false);
    },

    /**
     * If the page does not contain focused elements, focuses on the
     * first appropriate.
     */
    focus: function() {
      if (this.pageDiv.contains(document.activeElement))
        return;
      var elements = this.pageDiv.querySelectorAll(
        'input, list, select, textarea, button');
      for (var i = 0; i < elements.length; i++) {
        var element = elements[i];
        element.focus();
        if (document.activeElement == element)
          return;
      }
    },
  };

  // Export
  return {
    HelpBasePage: HelpBasePage
  };
});
