// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Responds to route changes by expanding, collapsing, or scrolling to sections
 * on the page. Expanded sections take up the full height of the container. At
 * most one section should be expanded at any given time.
 * @polymerBehavior MainPageBehavior
 */
var MainPageBehaviorImpl = {
  /** @type {?HTMLElement} The scrolling container. */
  scroller: null,

  /** @override */
  attached: function() {
    if (this.domHost && this.domHost.parentNode.tagName == 'PAPER-HEADER-PANEL')
      this.scroller = this.domHost.parentNode.scroller;
    else
      this.scroller = document.body; // Used in unit tests.
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    // Scroll to the section except for back/forward. Also scroll for any
    // back/forward navigations that are in-page.
    var oldRouteWasSection =
        !!oldRoute && !!oldRoute.parent && !!oldRoute.section &&
        oldRoute.parent.section != oldRoute.section;

    if (oldRouteWasSection && newRoute == settings.Route.BASIC) {
      this.scroller.scrollTop = 0;
      return;
    }

    var scrollToSection =
        !settings.lastRouteChangeWasPopstate() || oldRouteWasSection;

    // For previously uncreated pages (including on first load), allow the page
    // to render before scrolling to or expanding the section.
    if (!oldRoute || this.scrollHeight == 0)
      setTimeout(this.tryTransitionToSection_.bind(this, scrollToSection));
    else
      this.tryTransitionToSection_(scrollToSection);
  },

  /**
   * If possible, transitions to the current route's section (by expanding or
   * scrolling to it). If another transition is running, finishes or cancels
   * that one, then schedules this function again. This ensures the current
   * section is quickly shown, without getting the page into a broken state --
   * if currentRoute changes in between calls, just transition to the new route.
   * @param {boolean} scrollToSection
   * @private
   */
  tryTransitionToSection_: function(scrollToSection) {
    var currentRoute = settings.getCurrentRoute();
    var currentSection = this.getSection(currentRoute.section);

    // If an animation is already playing, try finishing or canceling it.
    if (this.currentAnimation_) {
      this.maybeStopCurrentAnimation_();
      // Either way, this function will be called again once the current
      // animation ends.
      return;
    }

    var promise;
    var expandedSection = /** @type {?SettingsSectionElement} */(
        this.$$('settings-section.expanded'));
    if (expandedSection) {
      // If the section shouldn't be expanded, collapse it.
      if (!currentRoute.isSubpage() || expandedSection != currentSection) {
        promise = this.collapseSection_(expandedSection);
        // Scroll to the collapsed section.
        if (currentSection && scrollToSection)
          currentSection.scrollIntoView();
      } else {
        // Scroll to top while sliding to another subpage.
        this.scroller.scrollTop = 0;
      }
    } else if (currentSection) {
      // Expand the section into a subpage or scroll to it on the main page.
      if (currentRoute.isSubpage())
        promise = this.expandSection_(currentSection);
      else if (scrollToSection)
        currentSection.scrollIntoView();
    }

    // When this animation ends, another may be necessary. Call this function
    // again after the promise resolves.
    if (promise)
      promise.then(this.tryTransitionToSection_.bind(this, scrollToSection));
  },

  /**
   * If the current animation is inconsistent with the current route, stops the
   * animation by finishing or canceling it so the new route can be animated to.
   * @private
   */
  maybeStopCurrentAnimation_: function() {
    var currentRoute = settings.getCurrentRoute();
    var animatingSection = /** @type {?SettingsSectionElement} */(
        this.$$('settings-section.expanding, settings-section.collapsing'));
    assert(animatingSection);

    if (animatingSection.classList.contains('expanding')) {
      // Cancel the animation to go back to the main page if the animating
      // section shouldn't be expanded.
      if (animatingSection.section != currentRoute.section ||
          !currentRoute.isSubpage()) {
        this.currentAnimation_.cancel();
      }
      // Otherwise, let the expand animation continue.
      return;
    }

    assert(animatingSection.classList.contains('collapsing'));
    if (!currentRoute.isSubpage())
      return;

    // If the collapsing section actually matches the current route's section,
    // we can just cancel the animation to re-expand the section.
    if (animatingSection.section == currentRoute.section) {
      this.currentAnimation_.cancel();
      return;
    }

    // The current route is a subpage, so that section needs to expand.
    // Immediately finish the current collapse animation so that can happen.
    this.currentAnimation_.finish();
  },

  /**
   * Animates the card in |section|, expanding it to fill the page.
   * @param {!SettingsSectionElement} section
   * @return {!Promise} Resolved when the transition is finished or canceled.
   * @private
   */
  expandSection_: function(section) {
    assert(this.scroller);

    if (!section.canAnimateExpand()) {
      // Try to wait for the section to be created.
      return new Promise(function(resolve, reject) {
        setTimeout(resolve);
      });
    }

    // Save the scroller position before freezing it.
    this.origScrollTop_ = this.scroller.scrollTop;
    this.fire('freeze-scroll', true);

    // Freeze the section's height so its card can be removed from the flow.
    section.setFrozen(true);

    this.currentAnimation_ = section.animateExpand(this.scroller);

    var finished;
    return this.currentAnimation_.finished.then(function() {
      // Hide other sections and scroll to the top of the subpage.
      this.classList.add('showing-subpage');
      this.toggleOtherSectionsHidden_(section.section, true);
      this.scroller.scrollTop = 0;
      section.setFrozen(false);

      // Notify that the page is fully expanded.
      this.fire('subpage-expand');

      finished = true;
    }.bind(this), function() {
      // The animation was canceled; restore the section and scroll position.
      section.setFrozen(false);
      this.scroller.scrollTop = this.origScrollTop_;

      finished = false;
    }.bind(this)).then(function() {
      this.fire('freeze-scroll', false);
      this.currentAnimation_ = null;
    }.bind(this));
  },

  /**
   * Animates the card in |section|, collapsing it back into its section.
   * @param {!SettingsSectionElement} section
   * @return {!Promise} Resolved when the transition is finished or canceled.
   */
  collapseSection_: function(section) {
    assert(this.scroller);
    assert(section.classList.contains('expanded'));

    // Don't animate the collapse if we are transitioning between Basic/Advanced
    // and About, since the section won't be visible.
    var needAnimate =
        settings.Route.ABOUT.contains(settings.getCurrentRoute()) ==
        (section.domHost.tagName == 'SETTINGS-ABOUT-PAGE');

    // Animate the collapse if the section knows the original height, except
    // when switching between Basic/Advanced and About.
    var shouldAnimateCollapse = needAnimate && section.canAnimateCollapse();
    if (shouldAnimateCollapse) {
      this.fire('freeze-scroll', true);
      // Do the initial collapse setup, which takes the section out of the flow,
      // before showing everything.
      section.setUpAnimateCollapse(this.scroller);
    } else {
      section.classList.remove('expanded');
    }

    // Show everything.
    this.toggleOtherSectionsHidden_(section.section, false);
    this.classList.remove('showing-subpage');

    if (!shouldAnimateCollapse) {
      // Finish by restoring the section into the page.
      section.setFrozen(false);
      return Promise.resolve();
    }

    // Play the actual collapse animation.
    return new Promise(function(resolve, reject) {
      // Wait for the other sections to show up so we can scroll properly.
      setTimeout(function() {
        var newSection = settings.getCurrentRoute().section &&
            this.getSection(settings.getCurrentRoute().section);

        this.scroller.scrollTop = this.origScrollTop_;

        this.currentAnimation_ = section.animateCollapse(
            /** @type {!HTMLElement} */(this.scroller));

        this.currentAnimation_.finished.catch(function() {
          // The collapse was canceled, so the page is showing a subpage still.
          this.fire('subpage-expand');
        }.bind(this)).then(function() {
          // Clean up after the animation succeeds or cancels.
          section.setFrozen(false);
          section.classList.remove('collapsing');
          this.fire('freeze-scroll', false);
          this.currentAnimation_ = null;
          resolve();
        }.bind(this));
      }.bind(this));
    }.bind(this));
  },

  /**
  /**
   * Hides or unhides the sections not being expanded.
   * @param {string} sectionName The section to keep visible.
   * @param {boolean} hidden Whether the sections should be hidden.
   * @private
   */
  toggleOtherSectionsHidden_: function(sectionName, hidden) {
    var sections = Polymer.dom(this.root).querySelectorAll(
        'settings-section');
    for (var section of sections)
      section.hidden = hidden && (section.section != sectionName);
  },

  /**
   * Helper function to get a section from the local DOM.
   * @param {string} section Section name of the element to get.
   * @return {?SettingsSectionElement}
   */
  getSection: function(section) {
    if (!section)
      return null;
    return /** @type {?SettingsSectionElement} */(
        this.$$('settings-section[section="' + section + '"]'));
  },
};

/** @polymerBehavior */
var MainPageBehavior = [
  settings.RouteObserverBehavior,
  MainPageBehaviorImpl,
];
