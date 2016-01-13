// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview New tab page
 * This is the main code for the new tab page used by touch-enabled Chrome
 * browsers.  For now this is still a prototype.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
cr.define('ntp', function() {
  'use strict';

  /**
   * NewTabView instance.
   * @type {!Object|undefined}
   */
  var newTabView;

  /**
   * The 'notification-container' element.
   * @type {!Element|undefined}
   */
  var notificationContainer;

  /**
   * If non-null, an info bubble for showing messages to the user. It points at
   * the Most Visited label, and is used to draw more attention to the
   * navigation dot UI.
   * @type {!Element|undefined}
   */
  var promoBubble;

  /**
   * If non-null, an bubble confirming that the user has signed into sync. It
   * points at the login status at the top of the page.
   * @type {!Element|undefined}
   */
  var loginBubble;

  /**
   * true if |loginBubble| should be shown.
   * @type {boolean}
   */
  var shouldShowLoginBubble = false;

  /**
   * The 'other-sessions-menu-button' element.
   * @type {!Element|undefined}
   */
  var otherSessionsButton;

  /**
   * The time when all sections are ready.
   * @type {number|undefined}
   * @private
   */
  var startTime;

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in new_tab.css.  Unfortunately there's no better way to try to time
   * something to occur until after a transition has completed.
   * @type {number}
   * @const
   */
  var DEFAULT_TRANSITION_TIME = 500;

  /**
   * See description for these values in ntp_stats.h.
   * @enum {number}
   */
  var NtpFollowAction = {
    CLICKED_TILE: 11,
    CLICKED_OTHER_NTP_PANE: 12,
    OTHER: 13
  };

  /**
   * Creates a NewTabView object. NewTabView extends PageListView with
   * new tab UI specific logics.
   * @constructor
   * @extends {PageListView}
   */
  function NewTabView() {
    var pageSwitcherStart = null;
    var pageSwitcherEnd = null;
    if (loadTimeData.getValue('showApps')) {
      pageSwitcherStart = getRequiredElement('page-switcher-start');
      pageSwitcherEnd = getRequiredElement('page-switcher-end');
    }
    this.initialize(getRequiredElement('page-list'),
                    getRequiredElement('dot-list'),
                    getRequiredElement('card-slider-frame'),
                    getRequiredElement('trash'),
                    pageSwitcherStart, pageSwitcherEnd);
  }

  NewTabView.prototype = {
    __proto__: ntp.PageListView.prototype,

    /** @override */
    appendTilePage: function(page, title, titleIsEditable, opt_refNode) {
      ntp.PageListView.prototype.appendTilePage.apply(this, arguments);

      if (promoBubble)
        window.setTimeout(promoBubble.reposition.bind(promoBubble), 0);
    }
  };

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function onLoad() {
    sectionsToWaitFor = 0;
    if (loadTimeData.getBoolean('showMostvisited'))
      sectionsToWaitFor++;
    if (loadTimeData.getBoolean('showApps')) {
      sectionsToWaitFor++;
      if (loadTimeData.getBoolean('showAppLauncherPromo')) {
        $('app-launcher-promo-close-button').addEventListener('click',
            function() { chrome.send('stopShowingAppLauncherPromo'); });
        $('apps-promo-learn-more').addEventListener('click',
            function() { chrome.send('onLearnMore'); });
      }
    }
    if (loadTimeData.getBoolean('isDiscoveryInNTPEnabled'))
      sectionsToWaitFor++;
    measureNavDots();

    // Load the current theme colors.
    themeChanged();

    newTabView = new NewTabView();

    notificationContainer = getRequiredElement('notification-container');
    notificationContainer.addEventListener(
        'webkitTransitionEnd', onNotificationTransitionEnd);

    if (loadTimeData.getBoolean('showRecentlyClosed')) {
      cr.ui.decorate($('recently-closed-menu-button'), ntp.RecentMenuButton);
      chrome.send('getRecentlyClosedTabs');
    } else {
      $('recently-closed-menu-button').hidden = true;
    }

    if (loadTimeData.getBoolean('showOtherSessionsMenu')) {
      otherSessionsButton = getRequiredElement('other-sessions-menu-button');
      cr.ui.decorate(otherSessionsButton, ntp.OtherSessionsMenuButton);
      otherSessionsButton.initialize(loadTimeData.getBoolean('isUserSignedIn'));
    } else {
      getRequiredElement('other-sessions-menu-button').hidden = true;
    }

    if (loadTimeData.getBoolean('showMostvisited')) {
      var mostVisited = new ntp.MostVisitedPage();
      // Move the footer into the most visited page if we are in "bare minimum"
      // mode.
      if (document.body.classList.contains('bare-minimum'))
        mostVisited.appendFooter(getRequiredElement('footer'));
      newTabView.appendTilePage(mostVisited,
                                loadTimeData.getString('mostvisited'),
                                false);
      chrome.send('getMostVisited');
    }

    if (loadTimeData.getBoolean('isDiscoveryInNTPEnabled')) {
      var suggestionsScript = document.createElement('script');
      suggestionsScript.src = 'suggestions_page.js';
      suggestionsScript.onload = function() {
         newTabView.appendTilePage(new ntp.SuggestionsPage(),
                                   loadTimeData.getString('suggestions'),
                                   false,
                                   (newTabView.appsPages.length > 0) ?
                                       newTabView.appsPages[0] : null);
         chrome.send('getSuggestions');
         cr.dispatchSimpleEvent(document, 'sectionready', true, true);
      };
      document.querySelector('head').appendChild(suggestionsScript);
    }

    if (!loadTimeData.getBoolean('showWebStoreIcon')) {
      var webStoreIcon = $('chrome-web-store-link');
      // Not all versions of the NTP have a footer, so this may not exist.
      if (webStoreIcon)
        webStoreIcon.hidden = true;
    } else {
      var webStoreLink = loadTimeData.getString('webStoreLink');
      var url = appendParam(webStoreLink, 'utm_source', 'chrome-ntp-launcher');
      $('chrome-web-store-link').href = url;
      $('chrome-web-store-link').addEventListener('click',
          onChromeWebStoreButtonClick);
    }

    // We need to wait for all the footer menu setup to be completed before
    // we can compute its layout.
    layoutFooter();

    if (loadTimeData.getString('login_status_message')) {
      loginBubble = new cr.ui.Bubble;
      loginBubble.anchorNode = $('login-container');
      loginBubble.arrowLocation = cr.ui.ArrowLocation.TOP_END;
      loginBubble.bubbleAlignment =
          cr.ui.BubbleAlignment.BUBBLE_EDGE_TO_ANCHOR_EDGE;
      loginBubble.deactivateToDismissDelay = 2000;
      loginBubble.closeButtonVisible = false;

      $('login-status-advanced').onclick = function() {
        chrome.send('showAdvancedLoginUI');
      };
      $('login-status-dismiss').onclick = loginBubble.hide.bind(loginBubble);

      var bubbleContent = $('login-status-bubble-contents');
      loginBubble.content = bubbleContent;

      // The anchor node won't be updated until updateLogin is called so don't
      // show the bubble yet.
      shouldShowLoginBubble = true;
    }

    if (loadTimeData.valueExists('bubblePromoText')) {
      promoBubble = new cr.ui.Bubble;
      promoBubble.anchorNode = getRequiredElement('promo-bubble-anchor');
      promoBubble.arrowLocation = cr.ui.ArrowLocation.BOTTOM_START;
      promoBubble.bubbleAlignment = cr.ui.BubbleAlignment.ENTIRELY_VISIBLE;
      promoBubble.deactivateToDismissDelay = 2000;
      promoBubble.content = parseHtmlSubset(
          loadTimeData.getString('bubblePromoText'), ['BR']);

      var bubbleLink = promoBubble.querySelector('a');
      if (bubbleLink) {
        bubbleLink.addEventListener('click', function(e) {
          chrome.send('bubblePromoLinkClicked');
        });
      }

      promoBubble.handleCloseEvent = function() {
        promoBubble.hide();
        chrome.send('bubblePromoClosed');
      };
      promoBubble.show();
      chrome.send('bubblePromoViewed');
    }

    var loginContainer = getRequiredElement('login-container');
    loginContainer.addEventListener('click', showSyncLoginUI);
    if (loadTimeData.getBoolean('shouldShowSyncLogin'))
      chrome.send('initializeSyncLogin');

    doWhenAllSectionsReady(function() {
      // Tell the slider about the pages.
      newTabView.updateSliderCards();
      // Mark the current page.
      newTabView.cardSlider.currentCardValue.navigationDot.classList.add(
          'selected');

      if (loadTimeData.valueExists('notificationPromoText')) {
        var promoText = loadTimeData.getString('notificationPromoText');
        var tags = ['IMG'];
        var attrs = {
          src: function(node, value) {
            return node.tagName == 'IMG' &&
                   /^data\:image\/(?:png|gif|jpe?g)/.test(value);
          },
        };

        var promo = parseHtmlSubset(promoText, tags, attrs);
        var promoLink = promo.querySelector('a');
        if (promoLink) {
          promoLink.addEventListener('click', function(e) {
            chrome.send('notificationPromoLinkClicked');
          });
        }

        showNotification(promo, [], function() {
          chrome.send('notificationPromoClosed');
        }, 60000);
        chrome.send('notificationPromoViewed');
      }

      cr.dispatchSimpleEvent(document, 'ntpLoaded', true, true);
      document.documentElement.classList.remove('starting-up');

      startTime = Date.now();
    });

    preventDefaultOnPoundLinkClicks();  // From webui/js/util.js.
    cr.ui.FocusManager.disableMouseFocusOnButtons();
  }

  /**
   * Launches the chrome web store app with the chrome-ntp-launcher
   * source.
   * @param {Event} e The click event.
   */
  function onChromeWebStoreButtonClick(e) {
    chrome.send('recordAppLaunchByURL',
                [encodeURIComponent(this.href),
                 ntp.APP_LAUNCH.NTP_WEBSTORE_FOOTER]);
  }

  /*
   * The number of sections to wait on.
   * @type {number}
   */
  var sectionsToWaitFor = -1;

  /**
   * Queued callbacks which lie in wait for all sections to be ready.
   * @type {array}
   */
  var readyCallbacks = [];

  /**
   * Fired as each section of pages becomes ready.
   * @param {Event} e Each page's synthetic DOM event.
   */
  document.addEventListener('sectionready', function(e) {
    if (--sectionsToWaitFor <= 0) {
      while (readyCallbacks.length) {
        readyCallbacks.shift()();
      }
    }
  });

  /**
   * This is used to simulate a fire-once event (i.e. $(document).ready() in
   * jQuery or Y.on('domready') in YUI. If all sections are ready, the callback
   * is fired right away. If all pages are not ready yet, the function is queued
   * for later execution.
   * @param {function} callback The work to be done when ready.
   */
  function doWhenAllSectionsReady(callback) {
    assert(typeof callback == 'function');
    if (sectionsToWaitFor > 0)
      readyCallbacks.push(callback);
    else
      window.setTimeout(callback, 0);  // Do soon after, but asynchronously.
  }

  /**
   * Measure the width of a nav dot with a given title.
   * @param {string} id The loadTimeData ID of the desired title.
   * @return {number} The width of the nav dot.
   */
  function measureNavDot(id) {
    var measuringDiv = $('fontMeasuringDiv');
    measuringDiv.textContent = loadTimeData.getString(id);
    // The 4 is for border and padding.
    return Math.max(measuringDiv.clientWidth * 1.15 + 4, 80);
  }

  /**
   * Fills in an invisible div with the longest dot title string so that
   * its length may be measured and the nav dots sized accordingly.
   */
  function measureNavDots() {
    var pxWidth = measureNavDot('appDefaultPageName');
    if (loadTimeData.getBoolean('showMostvisited'))
      pxWidth = Math.max(measureNavDot('mostvisited'), pxWidth);

    var styleElement = document.createElement('style');
    styleElement.type = 'text/css';
    // max-width is used because if we run out of space, the nav dots will be
    // shrunk.
    styleElement.textContent = '.dot { max-width: ' + pxWidth + 'px; }';
    document.querySelector('head').appendChild(styleElement);
  }

  /**
   * Layout the footer so that the nav dots stay centered.
   */
  function layoutFooter() {
    // We need the image to be loaded.
    var logo = $('logo-img');
    var logoImg = logo.querySelector('img');
    if (!logoImg.complete) {
      logoImg.onload = layoutFooter;
      return;
    }

    var menu = $('footer-menu-container');
    if (menu.clientWidth > logoImg.width)
      logo.style.WebkitFlex = '0 1 ' + menu.clientWidth + 'px';
    else
      menu.style.WebkitFlex = '0 1 ' + logoImg.width + 'px';
  }

  function themeChanged(opt_hasAttribution) {
    $('themecss').href = 'chrome://theme/css/new_tab_theme.css?' + Date.now();

    if (typeof opt_hasAttribution != 'undefined') {
      document.documentElement.setAttribute('hasattribution',
                                            opt_hasAttribution);
    }

    updateAttribution();
  }

  function setBookmarkBarAttached(attached) {
    document.documentElement.setAttribute('bookmarkbarattached', attached);
  }

  /**
   * Attributes the attribution image at the bottom left.
   */
  function updateAttribution() {
    var attribution = $('attribution');
    if (document.documentElement.getAttribute('hasattribution') == 'true') {
      attribution.hidden = false;
    } else {
      attribution.hidden = true;
    }
  }

  /**
   * Timeout ID.
   * @type {number}
   */
  var notificationTimeout = 0;

  /**
   * Shows the notification bubble.
   * @param {string|Node} message The notification message or node to use as
   *     message.
   * @param {Array.<{text: string, action: function()}>} links An array of
   *     records describing the links in the notification. Each record should
   *     have a 'text' attribute (the display string) and an 'action' attribute
   *     (a function to run when the link is activated).
   * @param {Function} opt_closeHandler The callback invoked if the user
   *     manually dismisses the notification.
   */
  function showNotification(message, links, opt_closeHandler, opt_timeout) {
    window.clearTimeout(notificationTimeout);

    var span = document.querySelector('#notification > span');
    if (typeof message == 'string') {
      span.textContent = message;
    } else {
      span.textContent = '';  // Remove all children.
      span.appendChild(message);
    }

    var linksBin = $('notificationLinks');
    linksBin.textContent = '';
    for (var i = 0; i < links.length; i++) {
      var link = linksBin.ownerDocument.createElement('div');
      link.textContent = links[i].text;
      link.action = links[i].action;
      link.onclick = function() {
        this.action();
        hideNotification();
      };
      link.setAttribute('role', 'button');
      link.setAttribute('tabindex', 0);
      link.className = 'link-button';
      linksBin.appendChild(link);
    }

    function closeFunc(e) {
      if (opt_closeHandler)
        opt_closeHandler();
      hideNotification();
    }

    document.querySelector('#notification button').onclick = closeFunc;
    document.addEventListener('dragstart', closeFunc);

    notificationContainer.hidden = false;
    showNotificationOnCurrentPage();

    newTabView.cardSlider.frame.addEventListener(
        'cardSlider:card_change_ended', onCardChangeEnded);

    var timeout = opt_timeout || 10000;
    notificationTimeout = window.setTimeout(hideNotification, timeout);
  }

  /**
   * Hide the notification bubble.
   */
  function hideNotification() {
    notificationContainer.classList.add('inactive');

    newTabView.cardSlider.frame.removeEventListener(
        'cardSlider:card_change_ended', onCardChangeEnded);
  }

  /**
   * Happens when 1 or more consecutive card changes end.
   * @param {Event} e The cardSlider:card_change_ended event.
   */
  function onCardChangeEnded(e) {
    // If we ended on the same page as we started, ignore.
    if (newTabView.cardSlider.currentCardValue.notification)
      return;

    // Hide the notification the old page.
    notificationContainer.classList.add('card-changed');

    showNotificationOnCurrentPage();
  }

  /**
   * Move and show the notification on the current page.
   */
  function showNotificationOnCurrentPage() {
    var page = newTabView.cardSlider.currentCardValue;
    doWhenAllSectionsReady(function() {
      if (page != newTabView.cardSlider.currentCardValue)
        return;

      // NOTE: This moves the notification to inside of the current page.
      page.notification = notificationContainer;

      // Reveal the notification and instruct it to hide itself if ignored.
      notificationContainer.classList.remove('inactive');

      // Gives the browser time to apply this rule before we remove it (causing
      // a transition).
      window.setTimeout(function() {
        notificationContainer.classList.remove('card-changed');
      }, 0);
    });
  }

  /**
   * When done fading out, set hidden to true so the notification can't be
   * tabbed to or clicked.
   * @param {Event} e The webkitTransitionEnd event.
   */
  function onNotificationTransitionEnd(e) {
    if (notificationContainer.classList.contains('inactive'))
      notificationContainer.hidden = true;
  }

  function setRecentlyClosedTabs(dataItems) {
    $('recently-closed-menu-button').dataItems = dataItems;
    layoutFooter();
  }

  function setMostVisitedPages(data, hasBlacklistedUrls) {
    newTabView.mostVisitedPage.data = data;
    cr.dispatchSimpleEvent(document, 'sectionready', true, true);
  }

  function setSuggestionsPages(data, hasBlacklistedUrls) {
    newTabView.suggestionsPage.data = data;
  }

  /**
   * Set the dominant color for a node. This will be called in response to
   * getFaviconDominantColor. The node represented by |id| better have a setter
   * for stripeColor.
   * @param {string} id The ID of a node.
   * @param {string} color The color represented as a CSS string.
   */
  function setFaviconDominantColor(id, color) {
    var node = $(id);
    if (node)
      node.stripeColor = color;
  }

  /**
   * Updates the text displayed in the login container. If there is no text then
   * the login container is hidden.
   * @param {string} loginHeader The first line of text.
   * @param {string} loginSubHeader The second line of text.
   * @param {string} iconURL The url for the login status icon. If this is null
        then the login status icon is hidden.
   * @param {boolean} isUserSignedIn Indicates if the user is signed in or not.
   */
  function updateLogin(loginHeader, loginSubHeader, iconURL, isUserSignedIn) {
    if (loginHeader || loginSubHeader) {
      $('login-container').hidden = false;
      $('login-status-header').innerHTML = loginHeader;
      $('login-status-sub-header').innerHTML = loginSubHeader;
      $('card-slider-frame').classList.add('showing-login-area');

      if (iconURL) {
        $('login-status-header-container').style.backgroundImage = url(iconURL);
        $('login-status-header-container').classList.add('login-status-icon');
      } else {
        $('login-status-header-container').style.backgroundImage = 'none';
        $('login-status-header-container').classList.remove(
            'login-status-icon');
      }
    } else {
      $('login-container').hidden = true;
      $('card-slider-frame').classList.remove('showing-login-area');
    }
    if (shouldShowLoginBubble) {
      window.setTimeout(loginBubble.show.bind(loginBubble), 0);
      chrome.send('loginMessageSeen');
      shouldShowLoginBubble = false;
    } else if (loginBubble) {
      loginBubble.reposition();
    }
    if (otherSessionsButton) {
      otherSessionsButton.updateSignInState(isUserSignedIn);
      layoutFooter();
    }
  }

  /**
   * Show the sync login UI.
   * @param {Event} e The click event.
   */
  function showSyncLoginUI(e) {
    var rect = e.currentTarget.getBoundingClientRect();
    chrome.send('showSyncLoginUI',
                [rect.left, rect.top, rect.width, rect.height]);
  }

  /**
   * Logs the time to click for the specified item.
   * @param {string} item The item to log the time-to-click.
   */
  function logTimeToClick(item) {
    var timeToClick = Date.now() - startTime;
    chrome.send('logTimeToClick',
        ['NewTabPage.TimeToClick' + item, timeToClick]);
  }

  /**
   * Wrappers to forward the callback to corresponding PageListView member.
   */
  function appAdded() {
    return newTabView.appAdded.apply(newTabView, arguments);
  }

  function appMoved() {
    return newTabView.appMoved.apply(newTabView, arguments);
  }

  function appRemoved() {
    return newTabView.appRemoved.apply(newTabView, arguments);
  }

  function appsPrefChangeCallback() {
    return newTabView.appsPrefChangedCallback.apply(newTabView, arguments);
  }

  function appLauncherPromoPrefChangeCallback() {
    return newTabView.appLauncherPromoPrefChangeCallback.apply(newTabView,
                                                               arguments);
  }

  function appsReordered() {
    return newTabView.appsReordered.apply(newTabView, arguments);
  }

  function enterRearrangeMode() {
    return newTabView.enterRearrangeMode.apply(newTabView, arguments);
  }

  function setForeignSessions(sessionList, isTabSyncEnabled) {
    if (otherSessionsButton) {
      otherSessionsButton.setForeignSessions(sessionList, isTabSyncEnabled);
      layoutFooter();
    }
  }

  function getAppsCallback() {
    return newTabView.getAppsCallback.apply(newTabView, arguments);
  }

  function getAppsPageIndex() {
    return newTabView.getAppsPageIndex.apply(newTabView, arguments);
  }

  function getCardSlider() {
    return newTabView.cardSlider;
  }

  function leaveRearrangeMode() {
    return newTabView.leaveRearrangeMode.apply(newTabView, arguments);
  }

  function saveAppPageName() {
    return newTabView.saveAppPageName.apply(newTabView, arguments);
  }

  function setAppToBeHighlighted(appId) {
    newTabView.highlightAppId = appId;
  }

  // Return an object with all the exports
  return {
    appAdded: appAdded,
    appMoved: appMoved,
    appRemoved: appRemoved,
    appsPrefChangeCallback: appsPrefChangeCallback,
    appLauncherPromoPrefChangeCallback: appLauncherPromoPrefChangeCallback,
    enterRearrangeMode: enterRearrangeMode,
    getAppsCallback: getAppsCallback,
    getAppsPageIndex: getAppsPageIndex,
    getCardSlider: getCardSlider,
    onLoad: onLoad,
    leaveRearrangeMode: leaveRearrangeMode,
    logTimeToClick: logTimeToClick,
    NtpFollowAction: NtpFollowAction,
    saveAppPageName: saveAppPageName,
    setAppToBeHighlighted: setAppToBeHighlighted,
    setBookmarkBarAttached: setBookmarkBarAttached,
    setForeignSessions: setForeignSessions,
    setMostVisitedPages: setMostVisitedPages,
    setSuggestionsPages: setSuggestionsPages,
    setRecentlyClosedTabs: setRecentlyClosedTabs,
    setFaviconDominantColor: setFaviconDominantColor,
    showNotification: showNotification,
    themeChanged: themeChanged,
    updateLogin: updateLogin
  };
});

document.addEventListener('DOMContentLoaded', ntp.onLoad);

var toCssPx = cr.ui.toCssPx;
