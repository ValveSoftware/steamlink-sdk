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

  // String specificators for different types of sessions.
  /** @const */ var SESSION_TYPE_GUEST = 'guest';
  /** @const */ var SESSION_TYPE_PUBLIC = 'public-account';

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
   * @return {boolean} Whether we're currently in guest session.
   */
  UIAccountTweaks.loggedInAsGuest = function() {
    return loadTimeData.getBoolean('loggedInAsGuest');
  };

  /**
   * @return {boolean} Whether we're currently in public session.
   */
  UIAccountTweaks.loggedInAsPublicAccount = function() {
    return loadTimeData.getBoolean('loggedInAsPublicAccount');
  };

  /**
   * @return {boolean} Whether we're currently in supervised user mode.
   */
  UIAccountTweaks.loggedInAsSupervisedUser = function() {
    return loadTimeData.getBoolean('loggedInAsSupervisedUser');
  };

  /**
   * Enables an element unless it should be disabled for the session type.
   *
   * @param {!Element} element Element that should be enabled.
   */
  UIAccountTweaks.enableElementIfPossible = function(element) {
    var sessionType;
    if (UIAccountTweaks.loggedInAsGuest())
      sessionType = SESSION_TYPE_GUEST;
    else if (UIAccountTweaks.loggedInAsPublicAccount())
      sessionType = SESSION_TYPE_PUBLIC;

    if (sessionType &&
        element.getAttribute(sessionType + '-visibility') == 'disabled') {
      return;
    }

    element.disabled = false;
  }

  /**
   * Disables or hides some elements in specified type of session in ChromeOS.
   * All elements within given document with *sessionType*-visibility
   * attribute are either hidden (for *sessionType*-visibility="hidden")
   * or disabled (for *sessionType*-visibility="disabled").
   *
   * @param {Document} document Document that should processed.
   * @param {string} sessionType name of the session type processed.
   * @private
   */
  UIAccountTweaks.applySessionTypeVisibility_ = function(document,
                                                         sessionType) {
    var elements = document.querySelectorAll('['+ sessionType + '-visibility]');
    for (var i = 0; i < elements.length; i++) {
      var element = elements[i];
      var visibility = element.getAttribute(sessionType + '-visibility');
      if (visibility == 'hidden')
        element.hidden = true;
      else if (visibility == 'disabled')
        UIAccountTweaks.disableElementsForSessionType(element, sessionType);
    }
  }

  /**
   * Updates specific visibility of elements for Guest session in ChromeOS.
   * Calls applySessionTypeVisibility_ method.
   *
   * @param {Document} document Document that should processed.
   */
  UIAccountTweaks.applyGuestSessionVisibility = function(document) {
    if (!UIAccountTweaks.loggedInAsGuest())
      return;
    UIAccountTweaks.applySessionTypeVisibility_(document, SESSION_TYPE_GUEST);
  }

  /**
   * Updates specific visibility of elements for Public account session in
   * ChromeOS. Calls applySessionTypeVisibility_ method.
   *
   * @param {Document} document Document that should processed.
   */
  UIAccountTweaks.applyPublicSessionVisibility = function(document) {
    if (!UIAccountTweaks.loggedInAsPublicAccount())
      return;
    UIAccountTweaks.applySessionTypeVisibility_(document, SESSION_TYPE_PUBLIC);
  }

  /**
   * Disables and marks page elements for specified session type.
   * Adds #-disabled css class to all elements within given subtree,
   * disables interactive elements (input/select/button), and removes href
   * attribute from <a> elements.
   *
   * @param {!Element} element Root element of DOM subtree that should be
   *     disabled.
   * @param {string} sessionType session type specificator.
   */
  UIAccountTweaks.disableElementsForSessionType = function(element,
                                                           sessionType) {
    UIAccountTweaks.disableElementForSessionType_(element, sessionType);

    // Walk the tree, searching each ELEMENT node.
    var walker = document.createTreeWalker(element,
                                           NodeFilter.SHOW_ELEMENT,
                                           null,
                                           false);

    var node = walker.nextNode();
    while (node) {
      UIAccountTweaks.disableElementForSessionType_(
          /** @type {!Element} */(node), sessionType);
      node = walker.nextNode();
    }
  };

  /**
   * Disables single element for given session type.
   * Adds *sessionType*-disabled css class, adds disabled attribute for
   * appropriate elements (input/select/button), and removes href attribute from
   * <a> element.
   *
   * @private
   * @param {!Element} element Element that should be disabled.
   * @param {string} sessionType account session Type specificator.
   */
  UIAccountTweaks.disableElementForSessionType_ = function(element,
                                                           sessionType) {
    element.classList.add(sessionType + '-disabled');
    if (element.nodeName == 'INPUT' ||
        element.nodeName == 'SELECT' ||
        element.nodeName == 'BUTTON') {
      element.disabled = true;
    } else if (element.nodeName == 'A') {
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
