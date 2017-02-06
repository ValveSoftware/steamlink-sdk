// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fast out, slow in.
var EASING_FUNCTION = 'cubic-bezier(0.4, 0, 0.2, 1)';
var EXPAND_DURATION = 350;

/**
 * Calls |readyTest| repeatedly until it returns true, then calls
 * |readyCallback|.
 * @param {function():boolean} readyTest
 * @param {!Function} readyCallback
 */
function doWhenReady(readyTest, readyCallback) {
  // TODO(dschuyler): Determine whether this hack can be removed.
  // See also: https://github.com/Polymer/polymer/issues/3629
  var intervalId = setInterval(function() {
    if (readyTest()) {
      clearInterval(intervalId);
      readyCallback();
    }
  }, 10);
}

/**
 * Provides animations to expand and collapse individual sections in a page.
 * Expanded sections take up the full height of the container. At most one
 * section should be expanded at any given time.
 * @polymerBehavior Polymer.MainPageBehavior
 */
var MainPageBehaviorImpl = {
  /**
   * @type {string} Selector to get the sections. Derived elements
   *     must override.
   */
  sectionSelector: '',

  /** @type {?Element} The scrolling container. */
  scroller: null,

  /** @override */
  attached: function() {
    this.scroller = this.domHost && this.domHost.parentNode.$.mainContainer;
  },

  /**
   * Hides or unhides the sections not being expanded.
   * @param {string} sectionName The section to keep visible.
   * @param {boolean} hidden Whether the sections should be hidden.
   * @private
   */
  toggleOtherSectionsHidden_: function(sectionName, hidden) {
    var sections = Polymer.dom(this.root).querySelectorAll(
        this.sectionSelector + ':not([section=' + sectionName + '])');
    for (var section of sections)
      section.hidden = hidden;
  },

  /**
   * Animates the card in |section|, expanding it to fill the page.
   * @param {!SettingsSectionElement} section
   */
  expandSection: function(section) {
    // If another section's card is expanding, cancel that animation first.
    var expanding = this.$$('.expanding');
    if (expanding) {
      if (expanding == section)
        return;

      if (this.animations['section']) {
        // Cancel the animation, then call startExpandSection_.
        this.cancelAnimation('section', function() {
          this.startExpandSection_(section);
        }.bind(this));
      } else {
        // The animation must have finished but its promise hasn't resolved yet.
        // When it resolves, collapse that section's card before expanding
        // this one.
        setTimeout(function() {
          this.collapseSection(
              /** @type {!SettingsSectionElement} */(expanding));
          this.finishAnimation('section', function() {
            this.startExpandSection_(section);
          }.bind(this));
        }.bind(this));
      }

      return;
    }

    if (this.$$('.collapsing') && this.animations['section']) {
      // Finish the collapse animation before expanding.
      this.finishAnimation('section', function() {
        this.startExpandSection_(section);
      }.bind(this));
      return;
    }

    this.startExpandSection_(section);
  },

  /**
   * Helper function to set up and start the expand animation.
   * @param {!SettingsSectionElement} section
   */
  startExpandSection_: function(section) {
    if (section.classList.contains('expanded'))
      return;

    // Freeze the scroller and save its position.
    this.listScrollTop_ = this.scroller.scrollTop;

    var scrollerWidth = this.scroller.clientWidth;
    this.scroller.style.overflow = 'hidden';
    // Adjust width to compensate for scroller.
    var scrollbarWidth = this.scroller.clientWidth - scrollerWidth;
    this.scroller.style.width = 'calc(100% - ' + scrollbarWidth + 'px)';

    // Freezes the section's height so its card can be removed from the flow.
    this.freezeSection_(section);

    // Expand the section's card to fill the parent.
    var animationPromise = this.playExpandSection_(section);

    animationPromise.then(function() {
      this.scroller.scrollTop = 0;
      this.toggleOtherSectionsHidden_(section.section, true);
    }.bind(this), function() {
      // Animation was canceled; restore the section.
      this.unfreezeSection_(section);
    }.bind(this)).then(function() {
      this.scroller.style.overflow = '';
      this.scroller.style.width = '';
    }.bind(this));
  },

  /**
   * Animates the card in |section|, collapsing it back into its section.
   * @param {!SettingsSectionElement} section
   */
  collapseSection: function(section) {
    // If the section's card is still expanding, cancel the expand animation.
    if (section.classList.contains('expanding')) {
      if (this.animations['section']) {
        this.cancelAnimation('section');
      } else {
        // The animation must have finished but its promise hasn't finished
        // resolving; try again asynchronously.
        this.async(function() {
          this.collapseSection(section);
        });
      }
      return;
    }

    if (!section.classList.contains('expanded'))
      return;

    this.toggleOtherSectionsHidden_(section.section, false);

    var scrollerWidth = this.scroller.clientWidth;
    this.scroller.style.overflow = 'hidden';
    // Adjust width to compensate for scroller.
    var scrollbarWidth = this.scroller.clientWidth - scrollerWidth;
    this.scroller.style.width = 'calc(100% - ' + scrollbarWidth + 'px)';

    this.playCollapseSection_(section).then(function() {
      this.unfreezeSection_(section);
      this.scroller.style.overflow = '';
      this.scroller.style.width = '';
      section.classList.remove('collapsing');
    }.bind(this));
  },

  /**
   * Freezes a section's height so its card can be removed from the flow without
   * affecting the layout of the surrounding sections.
   * @param {!SettingsSectionElement} section
   * @private
   */
  freezeSection_: function(section) {
    var card = section.$.card;
    section.style.height = section.clientHeight + 'px';

    var cardHeight = card.offsetHeight;
    var cardWidth = card.offsetWidth;
    // If the section is not displayed yet (e.g., navigated directly to a
    // sub-page), cardHeight and cardWidth are 0, so do not set the height or
    // width explicitly.
    // TODO(michaelpg): Improve this logic when refactoring
    // settings-animated-pages.
    if (cardHeight && cardWidth) {
      // TODO(michaelpg): Temporary hack to store the height the section should
      // collapse to when it closes.
      card.origHeight_ = cardHeight;

      card.style.height = cardHeight + 'px';
      card.style.width = cardWidth + 'px';
    } else {
      // Set an invalid value so we don't try to use it later.
      card.origHeight_ = NaN;
    }

    // Place the section's card at its current position but removed from the
    // flow.
    card.style.top = card.getBoundingClientRect().top + 'px';
    section.classList.add('frozen');
  },

  /**
   * After freezeSection_, restores the section to its normal height.
   * @param {!SettingsSectionElement} section
   * @private
   */
  unfreezeSection_: function(section) {
    if (!section.classList.contains('frozen'))
      return;
    var card = section.$.card;
    section.classList.remove('frozen');
    card.style.top = '';
    card.style.height = '';
    card.style.width = '';
    section.style.height = '';
  },

  /**
   * Expands the card in |section| to fill the page.
   * @param {!SettingsSectionElement} section
   * @return {!Promise}
   * @private
   */
  playExpandSection_: function(section) {
    var card = section.$.card;

    // The card should start at the top of the page.
    var targetTop = this.scroller.getBoundingClientRect().top;

    section.classList.add('expanding');

    // Expand the card, using minHeight. (The card must span the container's
    // client height, so it must be at least 100% in case the card is too short.
    // If the card is already taller than the container's client height, we
    // don't want to shrink the card to 100% or the content will overflow, so
    // we can't use height, and animating height wouldn't look right anyway.)
    var keyframes = [{
      top: card.style.top,
      minHeight: card.style.height,
      easing: EASING_FUNCTION,
    }, {
      top: targetTop + 'px',
      minHeight: 'calc(100% - ' + targetTop + 'px)',
    }];
    var options = /** @type {!KeyframeEffectOptions} */({
      duration: EXPAND_DURATION
    });
    // TODO(michaelpg): Change elevation of sections.
    var promise;
    if (keyframes[0].top && keyframes[0].minHeight)
      promise = this.animateElement('section', card, keyframes, options);
    else
      promise = Promise.resolve();

    promise.then(function() {
      section.classList.add('expanded');
      card.style.top = '';
      this.style.margin = 'auto';
      section.$.header.hidden = true;
      section.style.height = '';
    }.bind(this), function() {
      // The animation was canceled; catch the error and continue.
    }).then(function() {
      // Whether finished or canceled, clean up the animation.
      section.classList.remove('expanding');
      card.style.height = '';
      card.style.width = '';
    });

    return promise;
  },

  /**
   * Collapses the card in |section| back to its normal position.
   * @param {!SettingsSectionElement} section
   * @return {!Promise}
   * @private
   */
  playCollapseSection_: function(section) {
    var card = section.$.card;

    this.style.margin = '';
    section.$.header.hidden = false;

    var startingTop = this.scroller.getBoundingClientRect().top;

    var cardHeightStart = card.clientHeight;
    var cardWidthStart = card.clientWidth;

    section.classList.add('collapsing');
    section.classList.remove('expanding', 'expanded');

    // If we navigated here directly, we don't know the original height of the
    // section, so we skip the animation.
    // TODO(michaelpg): remove this condition once sliding is implemented.
    if (isNaN(card.origHeight_))
      return Promise.resolve();

    // Restore the section to its proper height to make room for the card.
    section.style.height = section.clientHeight + card.origHeight_ + 'px';

    // TODO(michaelpg): this should be in collapseSection(), but we need to wait
    // until the full page height is available (setting the section height).
    this.scroller.scrollTop = this.listScrollTop_;

    // The card is unpositioned, so use its position as the ending state,
    // but account for scroll.
    var targetTop = card.getBoundingClientRect().top - this.scroller.scrollTop;

    // Account for the section header.
    var headerStyle = getComputedStyle(section.$.header);
    targetTop += section.$.header.offsetHeight +
        parseInt(headerStyle.marginBottom, 10) +
        parseInt(headerStyle.marginTop, 10);

    var keyframes = [{
      top: startingTop + 'px',
      minHeight: cardHeightStart + 'px',
      easing: EASING_FUNCTION,
    }, {
      top: targetTop + 'px',
      minHeight: card.origHeight_ + 'px',
    }];
    var options = /** @type {!KeyframeEffectOptions} */({
      duration: EXPAND_DURATION
    });

    card.style.width = cardWidthStart + 'px';
    var promise = this.animateElement('section', card, keyframes, options);
    promise.then(function() {
      card.style.width = '';
    });
    return promise;
  },
};


/** @polymerBehavior */
var MainPageBehavior = [
  TransitionBehavior,
  MainPageBehaviorImpl
];


/**
 * TODO(michaelpg): integrate slide animations.
 * @polymerBehavior RoutableBehavior
 */
var RoutableBehaviorImpl = {
  properties: {
    /** Contains the current route. */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },
  },

  /** @private */
  scrollToSection_: function() {
    doWhenReady(
        function() {
          return this.scrollHeight > 0;
        }.bind(this),
        function() {
          // If the current section changes while we are waiting for the page to
          // be ready, scroll to the newest requested section.
          this.getSection_(this.currentRoute.section).scrollIntoView();
        }.bind(this));
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    var newRouteIsSubpage = newRoute && newRoute.subpage.length;
    var oldRouteIsSubpage = oldRoute && oldRoute.subpage.length;

    if (!oldRoute && newRouteIsSubpage) {
      // Allow the page to load before expanding the section. TODO(michaelpg):
      // Time this better when refactoring settings-animated-pages.
      setTimeout(function() {
        var section = this.getSection_(newRoute.section);
        if (section)
          this.expandSection(section);
      }.bind(this));
      return;
    }

    if (!newRouteIsSubpage && oldRouteIsSubpage) {
      var section = this.getSection_(oldRoute.section);
      if (section)
        this.collapseSection(section);
    } else if (newRouteIsSubpage &&
               (!oldRouteIsSubpage || newRoute.section != oldRoute.section)) {
      var section = this.getSection_(newRoute.section);
      if (section)
        this.expandSection(section);
    } else if (newRoute && newRoute.section &&
        this.$$('[data-page=' + newRoute.page + ']')) {
      this.scrollToSection_();
    }
  },

  /**
   * Helper function to get a section from the local DOM.
   * @param {string} section Section name of the element to get.
   * @return {?SettingsSectionElement}
   * @private
   */
  getSection_: function(section) {
    return /** @type {?SettingsSectionElement} */(
        this.$$('[section=' + section + ']'));
  },
};


/** @polymerBehavior */
var RoutableBehavior = [
  MainPageBehavior,
  RoutableBehaviorImpl
];
