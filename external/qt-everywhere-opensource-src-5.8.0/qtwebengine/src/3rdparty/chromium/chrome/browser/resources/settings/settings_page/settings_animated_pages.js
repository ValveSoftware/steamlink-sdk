// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-animated-pages' is a container for a page and animated subpages.
 * It provides a set of common behaviors and animations.
 *
 * Example:
 *
 *    <settings-animated-pages current-route="{{currentRoute}}"
 *        section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-animated-pages>
 */
Polymer({
  is: 'settings-animated-pages',

  properties: {
    /**
     * Contains the current route.
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },

    /**
     * Routes with this section activate this element. For instance, if this
     * property is 'search', and currentRoute.section is also set to 'search',
     * this element will display the subpage in currentRoute.subpage.
     *
     * The section name must match the name specified in settings_router.js.
     */
    section: {
      type: String,
    },
  },

  /** @override */
  created: function() {
    // Observe the light DOM so we know when it's ready.
    this.lightDomObserver_ = Polymer.dom(this).observeNodes(
        this.lightDomChanged_.bind(this));

    this.addEventListener('subpage-back', function() {
      assert(this.currentRoute.section == this.section);
      assert(this.currentRoute.subpage.length >= 1);

      this.setSubpageChain(this.currentRoute.subpage.slice(0, -1));
    }.bind(this));
  },

  /**
   * Called initially once the effective children are ready.
   * @private
   */
  lightDomChanged_: function() {
    if (this.lightDomReady_)
      return;

    this.lightDomReady_ = true;
    Polymer.dom(this).unobserveNodes(this.lightDomObserver_);
    this.runQueuedRouteChange_();
  },

  /**
   * Calls currentRouteChanged_ with the deferred route change info.
   * @private
   */
  runQueuedRouteChange_: function() {
    if (!this.queuedRouteChange_)
      return;
    this.async(this.currentRouteChanged_.bind(
        this,
        this.queuedRouteChange_.newRoute,
        this.queuedRouteChange_.oldRoute));
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    // Don't manipulate the light DOM until it's ready.
    if (!this.lightDomReady_) {
      this.queuedRouteChange_ = this.queuedRouteChange_ || {oldRoute: oldRoute};
      this.queuedRouteChange_.newRoute = newRoute;
      return;
    }

    var newRouteIsSubpage = newRoute && newRoute.subpage.length;
    var oldRouteIsSubpage = oldRoute && oldRoute.subpage.length;

    if (newRouteIsSubpage)
      this.ensureSubpageInstance_();

    if (!newRouteIsSubpage || !oldRouteIsSubpage ||
        newRoute.subpage.length == oldRoute.subpage.length) {
      // If two routes are at the same level, or if either the new or old route
      // is not a subpage, fade in and out.
      this.$.animatedPages.exitAnimation = 'fade-out-animation';
      this.$.animatedPages.entryAnimation = 'fade-in-animation';
    } else {
      // For transitioning between subpages at different levels, slide.
      if (newRoute.subpage.length > oldRoute.subpage.length) {
        this.$.animatedPages.exitAnimation = 'slide-left-animation';
        this.$.animatedPages.entryAnimation = 'slide-from-right-animation';
      } else {
        this.$.animatedPages.exitAnimation = 'slide-right-animation';
        this.$.animatedPages.entryAnimation = 'slide-from-left-animation';
      }
    }

    if (newRouteIsSubpage && newRoute.section == this.section)
      this.$.animatedPages.selected = newRoute.subpage.slice(-1)[0];
    else
      this.$.animatedPages.selected = 'main';
  },

  /**
   * Ensures that the template enclosing the subpage is stamped.
   * @private
   */
  ensureSubpageInstance_: function() {
    var id = this.currentRoute.subpage.slice(-1)[0];
    var template = Polymer.dom(this).querySelector(
        'template[name="' + id + '"]');

    // Nothing to do if the subpage isn't wrapped in a <template> or the
    // template is already stamped.
    if (!template || template.if)
      return;

    // Set the subpage's id for use by neon-animated-pages.
    var subpage = /** @type {{_content: DocumentFragment}} */(template)._content
        .querySelector('settings-subpage');
    if (!subpage.id)
      subpage.id = id;

    // Render synchronously so neon-animated-pages can select the subpage.
    template.if = true;
    template.render();
  },

  /**
   * Buttons in this pageset should use this method to transition to subpages.
   * @param {!Array<string>} subpage The chain of subpages within the page.
   */
  setSubpageChain: function(subpage) {
    var node = window.event.currentTarget;
    var page;
    while (node) {
      if (node.dataset && node.dataset.page)
        page = node.dataset.page;
      // A shadow root has a |host| rather than a |parentNode|.
      node = node.host || node.parentNode;
    }
    this.currentRoute = {
      page: page,
      section: subpage.length > 0 ? this.section : '',
      subpage: subpage,
    };
  },
});
