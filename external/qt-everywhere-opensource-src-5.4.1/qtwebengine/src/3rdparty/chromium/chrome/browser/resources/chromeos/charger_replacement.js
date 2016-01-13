// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chargerReplacement', function() {

  /**
   * Enumeration of charger selection.
   * @enum {int}
   */
  var CHARGER_SELECTION = {
    NONE: 0,
    GOOD_CHARGER: 1,
    ORIGINAL_CHARGER: 2
  };

  /**
   * Enumeration of html pages.
   * @enum {int}
   */
  var PAGES = {
    CHECK_CHARGER: 0,
    CONFIRM_SAFE_CHARGER: 1,
    CHARGER_UPDATE: 2,
    ORDER_CHARGER_ONLINE: 3,
    CONFIRM_ONLINE_ORDER: 4,
    FINISH_NOT_ORDER_CHARGER: 5,
    ORDER_CHARGER_OFFLINE: 6,
    CHARGER_STILL_BAD: 7,
  };

  /**
   *  Enumeration of the user's choice for order new charger or not.
   */
  var USER_CHOICE_FOR_CHARGER_UPDATE = {
    ORDER_NEW_CHARGER: 0,
    NOT_ORDER_NEW_CHARGER: 1,
  };

  /**
   *  Enumeration of messages sent from iFrame order form.
   */
  var ORDER_CHARGER_IFRAME_MESSAGE = {
    FORM_OPEN: 'FORM_OPEN',
    SUBMIT: 'SUBMIT',
    SUCCESS: 'SUCCESS',
    FAILURE: 'FAILURE',
    LINK: 'LINK',
  };

  /**
   *  Enumeration of countries where user might purchase the device.
   */
  var COUNTRY = {
    AU: 'au',
    CA: 'ca',
    IRE: 'ire',
    UK: 'uk',
    US: 'us',
  };

  /**
   *  Dialog argument passed from chrome to indicate whether user has ordered
   *  new charger.
   */
  var NEW_CHARGER_ORDERED = '1';

  /**
   *  Charger order form iFrame url.
   */
  var ORDER_CHARGER_IFRAME_URL = 'https://chromesafetycheck.appspot.com';

  /**
   *  maximum delay in milliseconds for loading the online charger order form
   *  into iFrame.
   */
  var ONLINE_ORDER_FORM_LOADING_DELAY = 30000;

  /**
   *  maximum delay in milliseconds for server to respond after user submits
   *  the order form.
   */
  var ONLINE_ORDER_SUBMISSION_DELAY = 60000;

  /**
   *  urls of href links on UI.
   */
  var CHARGER_FAQ_LINK = 'http://chromebook.com/hp11charger/';
  var PRIVACY_POLICY_LINK = 'http://www.google.com/policies/privacy';

  var onlineOrderSubmitTimer;

  /**
   *  flag for whether the online charger order form is loaded.
   */
  var isOrderFormLoaded = false;

  /**
   *  flag for whether user's online charger order form submission has been
   *  recevied by Google. True if the server responds with SUCCESSS or
   *  FAILURE. FAILURE indicate user has some input error in form and should
   *  submit again.
   */
  var isOrderSubmissionReceived = false;

  /**
   * charger selection by user.
   */
  var chargerSelection = CHARGER_SELECTION.NONE;

  /**
   * Set window the specified size and center it to screen.
   */
  function setWindowSizeAndCenter(width, height) {
    window.resizeTo(width, height);
    window.moveTo(window.screen.width / 2 - width / 2,
                  window.screen.height / 2 - height / 2);
  }

  /**
   * Show a particular page.
   */
  function showPage(page) {
    switch (page) {
      case PAGES.CHECK_CHARGER:
        setWindowSizeAndCenter(500, 590);
        break;
      case PAGES.CONFIRM_SAFE_CHARGER:
        setWindowSizeAndCenter(400, 325);
        break;
      case PAGES.CHARGER_UPDATE:
        setWindowSizeAndCenter(510, 505);
        break;
      case PAGES.ORDER_CHARGER_ONLINE:
        $('charger-order-form').src = ORDER_CHARGER_IFRAME_URL;
        setWindowSizeAndCenter(1150, 550);
        setTimeout(checkOnlineOrderFormLoaded, ONLINE_ORDER_FORM_LOADING_DELAY);
        break;
      case PAGES.CONFIRM_ONLINE_ORDER:
        setWindowSizeAndCenter(420, 380);
        break;
      case PAGES.FINISH_NOT_ORDER_CHARGER:
        setWindowSizeAndCenter(430, 350);
        break;
      case PAGES.ORDER_CHARGER_OFFLINE:
        setWindowSizeAndCenter(750, 600);
        break;
      case PAGES.CHARGER_STILL_BAD:
        setWindowSizeAndCenter(430, 380);
        break;
    }
    document.body.setAttribute('page', page);
  }

  /**
   *  Select a country from the drop down list.
   */
  function selectCountry() {
    var country = $('select-device-country').value;
    if (country == COUNTRY.US || country == COUNTRY.CA) {
      $('new-charger').src = $('new-charger-us').src;
      $('original-charger').src = $('original-charger-us').src;
    } else if (country == COUNTRY.AU) {
      $('new-charger').src = $('new-charger-au').src;
      $('original-charger').src = $('original-charger-au').src;
    } else {
      $('new-charger').src = $('new-charger-uk').src;
      $('original-charger').src = $('original-charger-uk').src;
    }
    $('charger-selection-strip').style.visibility = 'visible';
  }

  /**
   *  Toggle charger box border color based on if it is selected.
   */
  function ToggleChargerSelection(charger, selected) {
    charger.classList.toggle('selected-charger', selected);
    charger.classList.toggle('de-selected-charger', !selected);
  }

  /**
   * Select a charger, either original or good charger with green sticker.
   */
  function selectCharger(selection) {
    if (selection == CHARGER_SELECTION.NONE)
      return;

    chargerSelection = selection;
    $('check-charger-next-step').disabled = false;
    if (chargerSelection == CHARGER_SELECTION.GOOD_CHARGER) {
      var selectedCharger = $('new-charger-box');
      var notSelectedCharger = $('original-charger-box');
    } else {
      var selectedCharger = $('original-charger-box');
      var notSelectedCharger = $('new-charger-box');
    }
    ToggleChargerSelection(selectedCharger, true);
    ToggleChargerSelection(notSelectedCharger, false);
  }

  /**
   * Process the flow after user select a charger.
   */
  function afterSelectCharger(dialogArg) {
    if (chargerSelection == CHARGER_SELECTION.NONE)
      return;

    if (chargerSelection == CHARGER_SELECTION.GOOD_CHARGER) {
      showPage(PAGES.CONFIRM_SAFE_CHARGER);
    } else {
      if (dialogArg == NEW_CHARGER_ORDERED)
        showPage(PAGES.CHARGER_STILL_BAD);
      else
        showPage(PAGES.CHARGER_UPDATE);
    }
  }

  /**
   *  Proceed to next step after user make the choice for charger update.
   */
  function nextStepForChargerUpdate() {
    var radios = document.getElementsByName('order-new-charger');
    if (radios[USER_CHOICE_FOR_CHARGER_UPDATE.ORDER_NEW_CHARGER].checked) {
      if (navigator.onLine)
        showPage(PAGES.ORDER_CHARGER_ONLINE);
      else
        showPage(PAGES.ORDER_CHARGER_OFFLINE);
    } else {
      showPage(PAGES.FINISH_NOT_ORDER_CHARGER);
    }
  }

  /**
   *  Update the UI after user confirms the choice for charger update.
   */
  function afterUserConfirmationForChargerUpdate() {
    if ($('order-new-charger').checked) {
      $('not-order-charger-checkbox-strip').style.visibility = 'hidden';
      $('next-to-charger-update').disabled = false;
    } else {
      $('not-order-charger-checkbox-strip').style.visibility = 'visible';
      $('next-to-charger-update').disabled =
          !$('confirm-not-order-charger').checked;
    }
  }

  /**
   *  Check if the online order form has been loaded in iFrame.
   */
  function checkOnlineOrderFormLoaded() {
    if (!isOrderFormLoaded)
      showPage(PAGES.ORDER_CHARGER_OFFLINE);
  }

  /**
   *  Check if the online charger order has been successful or not.
   */
  function checkOnlineOrderSubmissionResponse() {
    if (!isOrderSubmissionReceived)
      showPage(PAGES.ORDER_CHARGER_OFFLINE);
  }

  /**
   *  Handle the messages posted by the iFrame for online order form.
   */
  function handleWindowMessage(e) {
    if (e.origin != ORDER_CHARGER_IFRAME_URL)
      return;

    var type = e.data['type'];
    if (type == ORDER_CHARGER_IFRAME_MESSAGE.FORM_OPEN) {
      isOrderFormLoaded = true;
    } else if (type == ORDER_CHARGER_IFRAME_MESSAGE.SUBMIT) {
      if (onlineOrderSubmitTimer)
        clearTimeout(onlineOrderSubmitTimer);
      onlineOrderSubmitTimer = setTimeout(checkOnlineOrderSubmissionResponse,
                                          ONLINE_ORDER_SUBMISSION_DELAY);
    } else if (type == ORDER_CHARGER_IFRAME_MESSAGE.SUCCESS) {
      isOrderSubmissionReceived = true;
      showPage(PAGES.CONFIRM_ONLINE_ORDER);
    } else if (type == ORDER_CHARGER_IFRAME_MESSAGE.FAILURE) {
      isOrderSubmissionReceived = true;
    } else if (type == ORDER_CHARGER_IFRAME_MESSAGE.LINK) {
      chrome.send('showLink', [e.data['link']]);
    }
  }

  /**
   *  Page loaded.
   */
  function load() {
    var dialogArg = chrome.getVariableValue('dialogArguments');
    showPage(PAGES.CHECK_CHARGER);
    $('check-charger-next-step').disabled = true;
    $('charger-selection-strip').style.visibility = 'hidden';
    $('order-new-charger').checked = true;
    $('finish-offline-order').disabled = true;
    $('check-charger-next-step').onclick = function() {
      afterSelectCharger(dialogArg);
    };
    $('select-device-country').onchange = function() {
      selectCountry();
    };
    $('new-charger').onclick = function() {
      selectCharger(CHARGER_SELECTION.GOOD_CHARGER);
    };
    $('original-charger').onclick = function() {
      selectCharger(CHARGER_SELECTION.ORIGINAL_CHARGER);
    };
    $('back-to-check-charger').onclick = function() {
      showPage(PAGES.CHECK_CHARGER);
    };
    $('finish-safe-charger').onclick = function() {
      chrome.send('confirmSafeCharger');
      chrome.send('dialogClose');
    };
    $('not-order-charger-checkbox-strip').style.visibility = 'hidden';
    $('back-to-check-charger-from-charger-update').onclick = function() {
      showPage(PAGES.CHECK_CHARGER);
    };
    $('next-to-charger-update').onclick = function() {
      nextStepForChargerUpdate();
    };
    $('order-new-charger').onclick = function() {
      afterUserConfirmationForChargerUpdate();
    };
    $('not-order-new-charger').onclick = function() {
      afterUserConfirmationForChargerUpdate();
    };
    $('confirm-not-order-charger').onclick = function() {
      afterUserConfirmationForChargerUpdate();
    };
    $('finish-not-order-new-charger').onclick = function() {
      chrome.send('confirmNotOrderNewCharger');
      chrome.send('dialogClose');
    };
    $('finish-online-order').onclick = function() {
      chrome.send('confirmChargerOrderedOnline');
      chrome.send('dialogClose');
    };
    $('offline-order-confirm').onclick = function() {
      $('finish-offline-order').disabled = !$('offline-order-confirm').checked;
    };
    $('finish-offline-order').onclick = function() {
      chrome.send('confirmChargerOrderByPhone');
      chrome.send('dialogClose');
    };
    $('finish-still-bad-charger').onclick = function() {
      chrome.send('confirmStillUseBadCharger');
      chrome.send('dialogClose');
    };

    var links = document.getElementsByClassName('link');
    for (var i = 0; i < links.length; ++i) {
      if (links[i].id == 'privacy-link') {
        links[i].onclick = function() {
          chrome.send('showLink', [PRIVACY_POLICY_LINK]);
        };
      } else {
        links[i].onclick = function() {
          chrome.send('showLink', [CHARGER_FAQ_LINK]);
        };
      }
    }

    window.addEventListener('message', handleWindowMessage);
  }

  return {
    load: load
  };
});

document.addEventListener('DOMContentLoaded', chargerReplacement.load);

