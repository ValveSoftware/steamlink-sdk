// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of utility methods for UberPage and its contained
 *     pages.
 */

cr.define('uber', function() {

  /**
   * Fixed position header elements on the page to be shifted by handleScroll.
   * @type {NodeList}
   */
  var headerElements;

  /**
   * This should be called by uber content pages when DOM content has loaded.
   */
  function onContentFrameLoaded() {
    headerElements = document.getElementsByTagName('header');
    document.addEventListener('scroll', handleScroll);

    invokeMethodOnParent('ready');

    // Prevent the navigation from being stuck in a disabled state when a
    // content page is reloaded while an overlay is visible (crbug.com/246939).
    invokeMethodOnParent('stopInterceptingEvents');

    // Trigger the scroll handler to tell the navigation if our page started
    // with some scroll (happens when you use tab restore).
    handleScroll();

    window.addEventListener('message', handleWindowMessage);
  }

  /**
   * Handles scroll events on the document. This adjusts the position of all
   * headers and updates the parent frame when the page is scrolled.
   * @private
   */
  function handleScroll() {
    var scrollLeft = scrollLeftForDocument(document);
    var offset = scrollLeft * -1;
    for (var i = 0; i < headerElements.length; i++) {
      // As a workaround for http://crbug.com/231830, set the transform to
      // 'none' rather than 0px.
      headerElements[i].style.webkitTransform = offset ?
          'translateX(' + offset + 'px)' : 'none';
    }

    invokeMethodOnParent('adjustToScroll', scrollLeft);
  };

  /**
   * Handles 'message' events on window.
   * @param {Event} e The message event.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'frameSelected')
      handleFrameSelected();
    else if (e.data.method === 'mouseWheel')
      handleMouseWheel(e.data.params);
    else if (e.data.method === 'popState')
      handlePopState(e.data.params.state, e.data.params.path);
  }

  /**
   * This is called when a user selects this frame via the navigation bar
   * frame (and is triggered via postMessage() from the uber page).
   * @private
   */
  function handleFrameSelected() {
    setScrollTopForDocument(document, 0);
  }

  /**
   * Called when a user mouse wheels (or trackpad scrolls) over the nav frame.
   * The wheel event is forwarded here and we scroll the body.
   * There's no way to figure out the actual scroll amount for a given delta.
   * It differs for every platform and even initWebKitWheelEvent takes a
   * pixel amount instead of a wheel delta. So we just choose something
   * reasonable and hope no one notices the difference.
   * @param {Object} params A structure that holds wheel deltas in X and Y.
   */
  function handleMouseWheel(params) {
    window.scrollBy(-params.deltaX * 49 / 120, -params.deltaY * 49 / 120);
  }

  /**
   * Called when the parent window restores some state saved by uber.pushState
   * or uber.replaceState. Simulates a popstate event.
   */
  function handlePopState(state, path) {
    history.replaceState(state, '', path);
    window.dispatchEvent(new PopStateEvent('popstate', {state: state}));
  }

  /**
   * @return {boolean} Whether this frame has a parent.
   */
  function hasParent() {
    return window != window.parent;
  }

  /**
   * Invokes a method on the parent window (UberPage). This is a convenience
   * method for API calls into the uber page.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @private
   */
  function invokeMethodOnParent(method, opt_params) {
    if (!hasParent())
      return;

    invokeMethodOnWindow(window.parent, method, opt_params, 'chrome://chrome');
  }

  /**
   * Invokes a method on the target window.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @param {string=} opt_url The origin of the target window.
   * @private
   */
  function invokeMethodOnWindow(targetWindow, method, opt_params, opt_url) {
    var data = {method: method, params: opt_params};
    targetWindow.postMessage(data, opt_url ? opt_url : '*');
  }

  /**
   * Updates the page's history state. If the page is embedded in a child,
   * forward the information to the parent for it to manage history for us. This
   * is a replacement of history.replaceState and history.pushState.
   * @param {Object} state A state object for replaceState and pushState.
   * @param {string} title The title of the page to replace.
   * @param {string} path The path the page navigated to.
   * @param {boolean} replace If true, navigate with replacement.
   * @private
   */
  function updateHistory(state, path, replace) {
    var historyFunction = replace ?
        window.history.replaceState :
        window.history.pushState;

    if (hasParent()) {
      // If there's a parent, always replaceState. The parent will do the actual
      // pushState.
      historyFunction = window.history.replaceState;
      invokeMethodOnParent('updateHistory', {
        state: state, path: path, replace: replace});
    }
    historyFunction.call(window.history, state, '', '/' + path);
  }

  /**
   * Sets the current title for the page. If the page is embedded in a child,
   * forward the information to the parent. This is a replacement for setting
   * document.title.
   * @param {string} title The new title for the page.
   */
  function setTitle(title) {
    document.title = title;
    invokeMethodOnParent('setTitle', {title: title});
  }

  /**
   * Pushes new history state for the page. If the page is embedded in a child,
   * forward the information to the parent; when embedded, all history entries
   * are attached to the parent. This is a replacement of history.pushState.
   * @param {Object} state A state object for replaceState and pushState.
   * @param {string} path The path the page navigated to.
   */
  function pushState(state, path) {
    updateHistory(state, path, false);
  }

  /**
   * Replaces the page's history state. If the page is embedded in a child,
   * forward the information to the parent; when embedded, all history entries
   * are attached to the parent. This is a replacement of history.replaceState.
   * @param {Object} state A state object for replaceState and pushState.
   * @param {string} path The path the page navigated to.
   */
  function replaceState(state, path) {
    updateHistory(state, path, true);
  }

  return {
    invokeMethodOnParent: invokeMethodOnParent,
    invokeMethodOnWindow: invokeMethodOnWindow,
    onContentFrameLoaded: onContentFrameLoaded,
    pushState: pushState,
    replaceState: replaceState,
    setTitle: setTitle,
  };
});
