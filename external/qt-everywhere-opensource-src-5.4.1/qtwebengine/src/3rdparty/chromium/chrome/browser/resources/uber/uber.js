// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {
  /**
   * Options for how web history should be handled.
   */
  var HISTORY_STATE_OPTION = {
    PUSH: 1,    // Push a new history state.
    REPLACE: 2, // Replace the current history state.
    NONE: 3,    // Ignore this history state change.
  };

  /**
   * We cache a reference to the #navigation frame here so we don't need to grab
   * it from the DOM on each scroll.
   * @type {Node}
   * @private
   */
  var navFrame;

  /**
   * A queue of method invocations on one of the iframes; if the iframe has not
   * loaded by the time there is a method to invoke, delay the invocation until
   * it is ready.
   * @type {Object}
   * @private
   */
  var queuedInvokes = {};

  /**
   * Handles page initialization.
   */
  function onLoad(e) {
    navFrame = $('navigation');
    navFrame.dataset.width = navFrame.offsetWidth;

    // Select a page based on the page-URL.
    var params = resolvePageInfo();
    showPage(params.id, HISTORY_STATE_OPTION.NONE, params.path);

    window.addEventListener('message', handleWindowMessage);
    window.setTimeout(function() {
      document.documentElement.classList.remove('loading');
    }, 0);

    // HACK(dbeam): This makes the assumption that any second part to a path
    // will result in needing background navigation. We shortcut it to avoid
    // flicker on load.
    // HACK(csilv): Search URLs aren't overlays, special case them.
    if (params.id == 'settings' && params.path &&
        params.path.indexOf('search') != 0) {
      backgroundNavigation();
    }

    ensureNonSelectedFrameContainersAreHidden();
  }

  /**
   * Find page information from window.location. If the location doesn't
   * point to one of our pages, return default parameters.
   * @return {Object} An object containing the following parameters:
   *     id - The 'id' of the page.
   *     path - A path into the page, including search and hash. Optional.
   */
  function resolvePageInfo() {
    var params = {};
    var path = window.location.pathname;
    if (path.length > 1) {
      // Split the path into id and the remaining path.
      path = path.slice(1);
      var index = path.indexOf('/');
      if (index != -1) {
        params.id = path.slice(0, index);
        params.path = path.slice(index + 1);
      } else {
        params.id = path;
      }

      var container = $(params.id);
      if (container) {
        // The id is valid. Add the hash and search parts of the URL to path.
        params.path = (params.path || '') + window.location.search +
            window.location.hash;
      } else {
        // The target sub-page does not exist, discard the params we generated.
        params.id = undefined;
        params.path = undefined;
      }
    }
    // If we don't have a valid page, get a default.
    if (!params.id)
      params.id = getDefaultIframe().id;

    return params;
  }

  /**
   * Handler for window.onpopstate.
   * @param {Event} e The history event.
   */
  function onPopHistoryState(e) {
    // Use the URL to determine which page to route to.
    var params = resolvePageInfo();

    // If the page isn't the current page, load it fresh. Even if the page is
    // already loaded, it may have state not reflected in the URL, such as the
    // history page's "Remove selected items" overlay. http://crbug.com/377386
    if (getRequiredElement(params.id) !== getSelectedIframe())
      showPage(params.id, HISTORY_STATE_OPTION.NONE, params.path);

    // Either way, send the state down to it.
    //
    // Note: This assumes that the state and path parameters for every page
    // under this origin are compatible. All of the downstream pages which
    // navigate use pushState and replaceState.
    invokeMethodOnPage(params.id, 'popState',
                       {state: e.state, path: '/' + params.path});
  }

  /**
   * @return {Object} The default iframe container.
   */
  function getDefaultIframe() {
    return $(loadTimeData.getString('helpHost'));
  }

  /**
   * @return {Object} The currently selected iframe container.
   */
  function getSelectedIframe() {
    return document.querySelector('.iframe-container.selected');
  }

  /**
   * Handles postMessage calls from the iframes of the contained pages.
   *
   * The pages request functionality from this object by passing an object of
   * the following form:
   *
   *  { method : "methodToInvoke",
   *    params : {...}
   *  }
   *
   * |method| is required, while |params| is optional. Extra parameters required
   * by a method must be specified by that method's documentation.
   *
   * @param {Event} e The posted object.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'beginInterceptingEvents') {
      backgroundNavigation();
    } else if (e.data.method === 'stopInterceptingEvents') {
      foregroundNavigation();
    } else if (e.data.method === 'ready') {
      pageReady(e.origin);
    } else if (e.data.method === 'updateHistory') {
      updateHistory(e.origin, e.data.params.state, e.data.params.path,
                    e.data.params.replace);
    } else if (e.data.method === 'setTitle') {
      setTitle(e.origin, e.data.params.title);
    } else if (e.data.method === 'showPage') {
      showPage(e.data.params.pageId,
               HISTORY_STATE_OPTION.PUSH,
               e.data.params.path);
    } else if (e.data.method === 'navigationControlsLoaded') {
      onNavigationControlsLoaded();
    } else if (e.data.method === 'adjustToScroll') {
      adjustToScroll(e.data.params);
    } else if (e.data.method === 'mouseWheel') {
      forwardMouseWheel(e.data.params);
    } else {
      console.error('Received unexpected message', e.data);
    }
  }

  /**
   * Sends the navigation iframe to the background.
   */
  function backgroundNavigation() {
    navFrame.classList.add('background');
    navFrame.firstChild.tabIndex = -1;
    navFrame.firstChild.setAttribute('aria-hidden', true);
  }

  /**
   * Retrieves the navigation iframe from the background.
   */
  function foregroundNavigation() {
    navFrame.classList.remove('background');
    navFrame.firstChild.tabIndex = 0;
    navFrame.firstChild.removeAttribute('aria-hidden');
  }

  /**
   * Enables or disables animated transitions when changing content while
   * horizontally scrolled.
   * @param {boolean} enabled True if enabled, else false to disable.
   */
  function setContentChanging(enabled) {
    navFrame.classList[enabled ? 'add' : 'remove']('changing-content');

    if (isRTL()) {
      uber.invokeMethodOnWindow(navFrame.firstChild.contentWindow,
                                'setContentChanging',
                                enabled);
    }
  }

  /**
   * Get an iframe based on the origin of a received post message.
   * @param {string} origin The origin of a post message.
   * @return {!HTMLElement} The frame associated to |origin| or null.
   */
  function getIframeFromOrigin(origin) {
    assert(origin.substr(-1) != '/', 'invalid origin given');
    var query = '.iframe-container > iframe[src^="' + origin + '/"]';
    return document.querySelector(query);
  }

  /**
   * Changes the path past the page title (i.e. chrome://chrome/settings/(.*)).
   * @param {Object} state The page's state object for the navigation.
   * @param {string} path The new /path/ to be set after the page name.
   * @param {number} historyOption The type of history modification to make.
   */
  function changePathTo(state, path, historyOption) {
    assert(!path || path.substr(-1) != '/', 'invalid path given');

    var histFunc;
    if (historyOption == HISTORY_STATE_OPTION.PUSH)
      histFunc = window.history.pushState;
    else if (historyOption == HISTORY_STATE_OPTION.REPLACE)
      histFunc = window.history.replaceState;

    assert(histFunc, 'invalid historyOption given ' + historyOption);

    var pageId = getSelectedIframe().id;
    var args = [state, '', '/' + pageId + '/' + (path || '')];
    histFunc.apply(window.history, args);
  }

  /**
   * Adds or replaces the current history entry based on a navigation from the
   * source iframe.
   * @param {string} origin The origin of the source iframe.
   * @param {Object} state The source iframe's state object.
   * @param {string} path The new "path" (e.g. "/createProfile").
   * @param {boolean} replace Whether to replace the current history entry.
   */
  function updateHistory(origin, state, path, replace) {
    assert(!path || path[0] != '/', 'invalid path sent from ' + origin);
    var historyOption =
        replace ? HISTORY_STATE_OPTION.REPLACE : HISTORY_STATE_OPTION.PUSH;
    // Only update the currently displayed path if this is the visible frame.
    var container = getIframeFromOrigin(origin).parentNode;
    if (container == getSelectedIframe())
      changePathTo(state, path, historyOption);
  }

  /**
   * Sets the title of the page.
   * @param {string} origin The origin of the source iframe.
   * @param {string} title The title of the page.
   */
  function setTitle(origin, title) {
    // Cache the title for the client iframe, i.e., the iframe setting the
    // title. querySelector returns the actual iframe element, so use parentNode
    // to get back to the container.
    var container = getIframeFromOrigin(origin).parentNode;
    container.dataset.title = title;

    // Only update the currently displayed title if this is the visible frame.
    if (container == getSelectedIframe())
      document.title = title;
  }

  /**
   * Invokes a method on a subpage. If the subpage has not signaled readiness,
   * queue the message for when it does.
   * @param {string} pageId Should match an id of one of the iframe containers.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property page of parameters to pass to
   *     the invoked method.
   */
  function invokeMethodOnPage(pageId, method, opt_params) {
    var frame = $(pageId).querySelector('iframe');
    if (!frame || !frame.dataset.ready) {
      queuedInvokes[pageId] = (queuedInvokes[pageId] || []);
      queuedInvokes[pageId].push([method, opt_params]);
    } else {
      uber.invokeMethodOnWindow(frame.contentWindow, method, opt_params);
    }
  }

  /**
   * Called in response to a page declaring readiness. Calls any deferred method
   * invocations from invokeMethodOnPage.
   * @param {string} origin The origin of the source iframe.
   */
  function pageReady(origin) {
    var frame = getIframeFromOrigin(origin);
    var container = frame.parentNode;
    frame.dataset.ready = true;
    var queue = queuedInvokes[container.id] || [];
    queuedInvokes[container.id] = undefined;
    for (var i = 0; i < queue.length; i++) {
      uber.invokeMethodOnWindow(frame.contentWindow, queue[i][0], queue[i][1]);
    }
  }

  /**
   * Selects and navigates a subpage. This is called from uber-frame.
   * @param {string} pageId Should match an id of one of the iframe containers.
   * @param {integer} historyOption Indicates whether we should push or replace
   *     browser history.
   * @param {string} path A sub-page path.
   */
  function showPage(pageId, historyOption, path) {
    var container = $(pageId);

    // Lazy load of iframe contents.
    var sourceUrl = container.dataset.url + (path || '');
    var frame = container.querySelector('iframe');
    if (!frame) {
      frame = container.ownerDocument.createElement('iframe');
      frame.name = pageId;
      container.appendChild(frame);
      frame.src = sourceUrl;
    } else {
      // There's no particularly good way to know what the current URL of the
      // content frame is as we don't have access to its contentWindow's
      // location, so just replace every time until necessary to do otherwise.
      frame.contentWindow.location.replace(sourceUrl);
      frame.dataset.ready = false;
    }

    // If the last selected container is already showing, ignore the rest.
    var lastSelected = document.querySelector('.iframe-container.selected');
    if (lastSelected === container)
      return;

    if (lastSelected) {
      lastSelected.classList.remove('selected');
      // Setting aria-hidden hides the container from assistive technology
      // immediately. The 'hidden' attribute is set after the transition
      // finishes - that ensures it's not possible to accidentally focus
      // an element in an unselected container.
      lastSelected.setAttribute('aria-hidden', 'true');
    }

    // Containers that aren't selected have to be hidden so that their
    // content isn't focusable.
    container.hidden = false;
    container.setAttribute('aria-hidden', 'false');

    // Trigger a layout after making it visible and before setting
    // the class to 'selected', so that it animates in.
    container.offsetTop;
    container.classList.add('selected');

    setContentChanging(true);
    adjustToScroll(0);

    var selectedFrame = getSelectedIframe().querySelector('iframe');
    uber.invokeMethodOnWindow(selectedFrame.contentWindow, 'frameSelected');

    if (historyOption != HISTORY_STATE_OPTION.NONE)
      changePathTo({}, path, historyOption);

    if (container.dataset.title)
      document.title = container.dataset.title;
    $('favicon').href = 'chrome://theme/' + container.dataset.favicon;
    $('favicon2x').href = 'chrome://theme/' + container.dataset.favicon + '@2x';

    updateNavigationControls();
  }

  function onNavigationControlsLoaded() {
    updateNavigationControls();
  }

  /**
   * Sends a message to uber-frame to update the appearance of the nav controls.
   * It should be called whenever the selected iframe changes.
   */
  function updateNavigationControls() {
    var iframe = getSelectedIframe();
    uber.invokeMethodOnWindow(navFrame.firstChild.contentWindow,
                              'changeSelection', {pageId: iframe.id});
  }

  /**
   * Forwarded scroll offset from a content frame's scroll handler.
   * @param {number} scrollOffset The scroll offset from the content frame.
   */
  function adjustToScroll(scrollOffset) {
    // NOTE: The scroll is reset to 0 and easing turned on every time a user
    // switches frames. If we receive a non-zero value it has to have come from
    // a real user scroll, so we disable easing when this happens.
    if (scrollOffset != 0)
      setContentChanging(false);

    if (isRTL()) {
      uber.invokeMethodOnWindow(navFrame.firstChild.contentWindow,
                                'adjustToScroll',
                                scrollOffset);
      var navWidth = Math.max(0, +navFrame.dataset.width + scrollOffset);
      navFrame.style.width = navWidth + 'px';
    } else {
      navFrame.style.webkitTransform = 'translateX(' + -scrollOffset + 'px)';
    }
  }

  /**
   * Forward scroll wheel events to subpages.
   * @param {Object} params Relevant parameters of wheel event.
   */
  function forwardMouseWheel(params) {
    var iframe = getSelectedIframe().querySelector('iframe');
    uber.invokeMethodOnWindow(iframe.contentWindow, 'mouseWheel', params);
  }

  /**
   * Make sure that iframe containers that are not selected are
   * hidden, so that elements in those frames aren't part of the
   * focus order. Containers that are unselected later get hidden
   * when the transition ends. We also set the aria-hidden attribute
   * because that hides the container from assistive technology
   * immediately, rather than only after the transition ends.
   */
  function ensureNonSelectedFrameContainersAreHidden() {
    var containers = document.querySelectorAll('.iframe-container');
    for (var i = 0; i < containers.length; i++) {
      var container = containers[i];
      if (!container.classList.contains('selected')) {
        container.hidden = true;
        container.setAttribute('aria-hidden', 'true');
      }
      container.addEventListener('webkitTransitionEnd', function(event) {
        if (!event.target.classList.contains('selected'))
          event.target.hidden = true;
      });
    }
  }

  return {
    onLoad: onLoad,
    onPopHistoryState: onPopHistoryState
  };
});

window.addEventListener('popstate', uber.onPopHistoryState);
document.addEventListener('DOMContentLoaded', uber.onLoad);
