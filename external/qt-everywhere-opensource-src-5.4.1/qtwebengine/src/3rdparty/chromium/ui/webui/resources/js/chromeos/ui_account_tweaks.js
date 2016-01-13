// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains methods that allow to tweak
 * internal page UI based on the status of current user (owner/user/guest).
 * It is assumed that required data is passed via i18n strings
 * (using loadTimeData dictionary) that are filled with call to
 * AddAccountUITweaksLocalizedValues in ui_account_tweaks.cc.
 * It is also assumed that tweaked page has chrome://resources/css/widgets.css
 * included.
 */

cr.define('uiAccountTweaks', function() {

  /////////////////////////////////////////////////////////////////////////////
  // UIAccountTweaks class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function UIAccountTweaks() {
  }

  /**
   * @return {boolean} Whether the current user is owner or not.
   */
  UIAccountTweaks.currentUserIsOwner = function() {
    return loadTimeData.getBoolean('currentUserIsOwner');
  };

  /**
   * @return {boolean} Whether we're currently in guest mode.
   */
  UIAccountTweaks.loggedInAsGuest = function() {
    return loadTimeData.getBoolean('loggedInAsGuest');
  };

  /**
   * @return {boolean} Whether we're currently in supervised user mode.
   */
  UIAccountTweaks.loggedInAsLocallyManagedUser = function() {
    return loadTimeData.getBoolean('loggedInAsLocallyManagedUser');
  };

  /**
   * Disables or hides some elements in Guest mode in ChromeOS.
   * All elements within given document with guest-visibility
   * attribute are either hidden (for guest-visibility="hidden")
   * or disabled (for guest-visibility="disabled").
   *
   * @param {Document} document Document that should processed.
   */
  UIAccountTweaks.applyGuestModeVisibility = function(document) {
    if (!cr.isChromeOS || !UIAccountTweaks.loggedInAsGuest())
      return;
    var elements = document.querySelectorAll('[guest-visibility]');
    for (var i = 0; i < elements.length; i++) {
      var element = elements[i];
      var visibility = element.getAttribute('guest-visibility');
      if (visibility == 'hidden')
        element.hidden = true;
      else if (visibility == 'disabled')
        UIAccountTweaks.disableElementsForGuest(element);
    }
  }

  /**
   * Disables and marks page elements for Guest mode.
   * Adds guest-disabled css class to all elements within given subtree,
   * disables interactive elements (input/select/button), and removes href
   * attribute from <a> elements.
   *
   * @param {Element} element Root element of DOM subtree that should be
   *     disabled.
   */
  UIAccountTweaks.disableElementsForGuest = function(element) {
    UIAccountTweaks.disableElementForGuest_(element);

    // Walk the tree, searching each ELEMENT node.
    var walker = document.createTreeWalker(element,
                                           NodeFilter.SHOW_ELEMENT,
                                           null,
                                           false);

    var node = walker.nextNode();
    while (node) {
      UIAccountTweaks.disableElementForGuest_(node);
      node = walker.nextNode();
    }
  };

  /**
   * Disables single element for Guest mode.
   * Adds guest-disabled css class, adds disabled attribute for appropriate
   * elements (input/select/button), and removes href attribute from
   * <a> element.
   *
   * @private
   * @param {Element} element Element that should be disabled.
   */
  UIAccountTweaks.disableElementForGuest_ = function(element) {
    element.classList.add('guest-disabled');
    if (element.nodeName == 'INPUT' ||
        element.nodeName == 'SELECT' ||
        element.nodeName == 'BUTTON')
      element.disabled = true;
    if (element.nodeName == 'A') {
      element.onclick = function() {
        return false;
      };
    }
  };

  // Export
  return {
    UIAccountTweaks: UIAccountTweaks
  };

});
