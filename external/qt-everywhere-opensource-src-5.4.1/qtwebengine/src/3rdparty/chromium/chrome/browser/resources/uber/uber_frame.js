// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the navigation controls that are visible on the left side
// of the uber page. It exists separately from uber.js so that it may be loaded
// in an iframe. Iframes can be layered on top of each other, but not mixed in
// with page content, so all overlapping content on uber must be framed.

<include src="../../../../ui/webui/resources/js/util.js"></include>
<include src="uber_utils.js"></include>

cr.define('uber_frame', function() {

  /**
   * Handles page initialization.
   */
  function onLoad() {
    var navigationItems = document.querySelectorAll('li');

    for (var i = 0; i < navigationItems.length; ++i) {
      navigationItems[i].addEventListener('click', onNavItemClicked);
    }

    window.addEventListener('message', handleWindowMessage);
    uber.invokeMethodOnParent('navigationControlsLoaded');

    document.documentElement.addEventListener('mousewheel', onMouseWheel);
    cr.ui.FocusManager.disableMouseFocusOnButtons();
  }

  /**
   * Handles clicks on the navigation controls (switches the page and updates
   * the URL).
   * @param {Event} e The click event.
   */
  function onNavItemClicked(e) {
    // Though pointer-event: none; is applied to the .selected nav item, users
    // can still tab to them and press enter/space which simulates a click.
    if (e.target.classList.contains('selected'))
      return;

    // Extensions can override Uber content (e.g., if the user has a history
    // extension, it should display when the 'History' navigation is clicked).
    if (e.currentTarget.getAttribute('override') == 'yes') {
      window.open('chrome://' + e.currentTarget.getAttribute('controls'),
          '_blank');
      return;
    }

    uber.invokeMethodOnParent('showPage',
       {pageId: e.currentTarget.getAttribute('controls')});

    setSelection(e.currentTarget);
  }

  /**
   * Handles postMessage from chrome://chrome.
   * @param {Event} e The post data.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'changeSelection')
      changeSelection(e.data.params);
    else if (e.data.method === 'adjustToScroll')
      adjustToScroll(e.data.params);
    else if (e.data.method === 'setContentChanging')
      setContentChanging(e.data.params);
    else
      console.error('Received unexpected message', e.data);
  }

  /**
   * Changes the selected nav control.
   * @param {Object} params Must contain pageId.
   */
  function changeSelection(params) {
    var navItem =
        document.querySelector('li[controls="' + params.pageId + '"]');
    setSelection(navItem);
    showNavItems();
  }

  /**
   * @return {Element} The currently selected nav item, if any.
   */
  function getSelectedNavItem() {
    return document.querySelector('li.selected');
  }

  /**
   * Sets selection on the given nav item.
   * @param {Element} newSelection The item to be selected.
   */
  function setSelection(newSelection) {
    var lastSelectedNavItem = getSelectedNavItem();
    if (lastSelectedNavItem !== newSelection) {
      newSelection.classList.add('selected');
      if (lastSelectedNavItem)
        lastSelectedNavItem.classList.remove('selected');
    }
  }

  /**
   * Shows nav items belonging to the same group as the selected item.
   */
  function showNavItems() {
    var navItems = document.querySelectorAll('li');
    var selectedNavItem = getSelectedNavItem();
    assert(selectedNavItem);

    var selectedGroup = selectedNavItem.getAttribute('group');
    for (var i = 0; i < navItems.length; ++i) {
      navItems[i].hidden = navItems[i].getAttribute('group') != selectedGroup;
    }
  }

  /**
   * Adjusts this frame's content to scrolls from the outer frame. This is done
   * to obscure text in RTL as a user scrolls over the content of this frame (as
   * currently RTL scrollbars still draw on the right).
   * @param {number} scroll document.body.scrollLeft of the content frame.
   */
  function adjustToScroll(scrollLeft) {
    assert(isRTL());
    document.body.style.webkitTransform = 'translateX(' + -scrollLeft + 'px)';
  }

  /**
   * Enable/disable an animation to ease the nav bar back into view when
   * changing content while horizontally scrolled.
   * @param {boolean} enabled Whether easing should be enabled.
   */
  function setContentChanging(enabled) {
    assert(isRTL());
    document.documentElement.classList[enabled ? 'add' : 'remove'](
        'changing-content');
  }

  /**
   * Handles mouse wheels on the top level element. Forwards them to uber.js.
   * @param {Event} e The mouse wheel event.
   */
  function onMouseWheel(e) {
    uber.invokeMethodOnParent('mouseWheel',
        {deltaX: e.wheelDeltaX, deltaY: e.wheelDeltaY});
  }

  /**
   * @return {Element} The currently selected iframe container.
   * @private
   */
  function getSelectedIframe() {
    return document.querySelector('.iframe-container.selected');
  }

  /**
   * Finds the <li> element whose 'controls' attribute is |controls| and sets
   * its 'override' attribute to |override|.
   * @param {string} controls The value of the 'controls' attribute of the
   *     element to change.
   * @param {string} override The value to set for the 'override' attribute of
   *     that element (either 'yes' or 'no').
   */
  function setNavigationOverride(controls, override) {
    var navItem =
        document.querySelector('li[controls="' + controls + '"]');
    navItem.setAttribute('override', override);
  }

  return {
    onLoad: onLoad,
    setNavigationOverride: setNavigationOverride,
  };

});

document.addEventListener('DOMContentLoaded', uber_frame.onLoad);
