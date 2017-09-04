// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview The event page for Google Now for Chrome implementation.
 * The Google Now event page gets Google Now cards from the server and shows
 * them as Chrome notifications.
 * The service performs periodic updating of Google Now cards.
 * Each updating of the cards includes 4 steps:
 * 1. Processing requests for cards dismissals that are not yet sent to the
 *    server.
 * 2. Making a server request.
 * 3. Showing the received cards as notifications.
 */

// TODO(robliao): Decide what to do in incognito mode.

/**
 * Standard response code for successful HTTP requests. This is the only success
 * code the server will send.
 */
var HTTP_OK = 200;
var HTTP_NOCONTENT = 204;

var HTTP_BAD_REQUEST = 400;
var HTTP_UNAUTHORIZED = 401;
var HTTP_FORBIDDEN = 403;
var HTTP_METHOD_NOT_ALLOWED = 405;

var MS_IN_SECOND = 1000;
var MS_IN_MINUTE = 60 * 1000;

/**
 * Initial period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var INITIAL_POLLING_PERIOD_SECONDS = 5 * 60;  // 5 minutes

/**
 * Mininal period for polling for Google Now Notifications cards.
 */
var MINIMUM_POLLING_PERIOD_SECONDS = 5 * 60;  // 5 minutes

/**
 * Maximal period for polling for Google Now Notifications cards to use when the
 * period from the server is not available.
 */
var MAXIMUM_POLLING_PERIOD_SECONDS = 30 * 60;  // 30 minutes

/**
 * Initial period for polling for Google Now optin notification after push
 * messaging indicates Google Now is enabled.
 */
var INITIAL_OPTIN_RECHECK_PERIOD_SECONDS = 60;  // 1 minute

/**
 * Maximum period for polling for Google Now optin notification after push
 * messaging indicates Google Now is enabled. It is expected that the alarm
 * will be stopped after this.
 */
var MAXIMUM_OPTIN_RECHECK_PERIOD_SECONDS = 16 * 60;  // 16 minutes

/**
 * Initial period for retrying the server request for dismissing cards.
 */
var INITIAL_RETRY_DISMISS_PERIOD_SECONDS = 60;  // 1 minute

/**
 * Maximum period for retrying the server request for dismissing cards.
 */
var MAXIMUM_RETRY_DISMISS_PERIOD_SECONDS = 60 * 60;  // 1 hour

/**
 * Time we keep retrying dismissals.
 */
var MAXIMUM_DISMISSAL_AGE_MS = 24 * 60 * 60 * 1000; // 1 day

/**
 * Time we keep dismissals after successful server dismiss requests.
 */
var DISMISS_RETENTION_TIME_MS = 20 * 60 * 1000;  // 20 minutes

/**
 * Default period for checking whether the user is opted in to Google Now.
 */
var DEFAULT_OPTIN_CHECK_PERIOD_SECONDS = 60 * 60 * 24 * 7; // 1 week

/**
 * URL to open when the user clicked on a link for the our notification
 * settings.
 */
var SETTINGS_URL = 'https://support.google.com/chrome/?p=ib_google_now_welcome';

/**
 * GCM registration URL.
 */
var GCM_REGISTRATION_URL =
    'https://android.googleapis.com/gcm/googlenotification';

/**
 * DevConsole project ID for GCM API use.
 */
var GCM_PROJECT_ID = '437902709571';

/**
 * Number of cards that need an explanatory link.
 */
var EXPLANATORY_CARDS_LINK_THRESHOLD = 4;

/**
 * Names for tasks that can be created by the extension.
 */
var UPDATE_CARDS_TASK_NAME = 'update-cards';
var DISMISS_CARD_TASK_NAME = 'dismiss-card';
var RETRY_DISMISS_TASK_NAME = 'retry-dismiss';
var STATE_CHANGED_TASK_NAME = 'state-changed';
var SHOW_ON_START_TASK_NAME = 'show-cards-on-start';
var ON_PUSH_MESSAGE_START_TASK_NAME = 'on-push-message';

/**
 * Group as received from the server.
 *
 * @typedef {{
 *   nextPollSeconds: (string|undefined),
 *   rank: (number|undefined),
 *   requested: (boolean|undefined)
 * }}
 */
var ReceivedGroup;

/**
 * Server response with notifications and groups.
 *
 * @typedef {{
 *   googleNowDisabled: (boolean|undefined),
 *   groups: Object<ReceivedGroup>,
 *   notifications: Array<ReceivedNotification>
 * }}
 */
var ServerResponse;

/**
 * Notification group as the client stores it. |cardsTimestamp| and |rank| are
 * defined if |cards| is non-empty. |nextPollTime| is undefined if the server
 * (1) never sent 'nextPollSeconds' for the group or
 * (2) didn't send 'nextPollSeconds' with the last group update containing a
 *     cards update and all the times after that.
 *
 * @typedef {{
 *   cards: Array<ReceivedNotification>,
 *   cardsTimestamp: (number|undefined),
 *   nextPollTime: (number|undefined),
 *   rank: (number|undefined)
 * }}
 */
var StoredNotificationGroup;

/**
 * Pending (not yet successfully sent) dismissal for a received notification.
 * |time| is the moment when the user requested dismissal.
 *
 * @typedef {{
 *   chromeNotificationId: ChromeNotificationId,
 *   time: number,
 *   dismissalData: DismissalData
 * }}
 */
var PendingDismissal;

/**
 * Checks if a new task can't be scheduled when another task is already
 * scheduled.
 * @param {string} newTaskName Name of the new task.
 * @param {string} scheduledTaskName Name of the scheduled task.
 * @return {boolean} Whether the new task conflicts with the existing task.
 */
function areTasksConflicting(newTaskName, scheduledTaskName) {
  if (newTaskName == UPDATE_CARDS_TASK_NAME &&
      scheduledTaskName == UPDATE_CARDS_TASK_NAME) {
    // If a card update is requested while an old update is still scheduled, we
    // don't need the new update.
    return true;
  }

  if (newTaskName == RETRY_DISMISS_TASK_NAME &&
      (scheduledTaskName == UPDATE_CARDS_TASK_NAME ||
       scheduledTaskName == DISMISS_CARD_TASK_NAME ||
       scheduledTaskName == RETRY_DISMISS_TASK_NAME)) {
    // No need to schedule retry-dismiss action if another action that tries to
    // send dismissals is scheduled.
    return true;
  }

  return false;
}

var tasks = buildTaskManager(areTasksConflicting);

// Add error processing to API calls.
wrapper.instrumentChromeApiFunction('gcm.onMessage.addListener', 0);
wrapper.instrumentChromeApiFunction('gcm.register', 1);
wrapper.instrumentChromeApiFunction('gcm.unregister', 0);
wrapper.instrumentChromeApiFunction('metricsPrivate.getVariationParams', 1);
wrapper.instrumentChromeApiFunction('notifications.clear', 1);
wrapper.instrumentChromeApiFunction('notifications.create', 2);
wrapper.instrumentChromeApiFunction('notifications.getPermissionLevel', 0);
wrapper.instrumentChromeApiFunction('notifications.update', 2);
wrapper.instrumentChromeApiFunction('notifications.getAll', 0);
wrapper.instrumentChromeApiFunction(
    'notifications.onButtonClicked.addListener', 0);
wrapper.instrumentChromeApiFunction('notifications.onClicked.addListener', 0);
wrapper.instrumentChromeApiFunction('notifications.onClosed.addListener', 0);
wrapper.instrumentChromeApiFunction(
    'notifications.onPermissionLevelChanged.addListener', 0);
wrapper.instrumentChromeApiFunction(
    'notifications.onShowSettings.addListener', 0);
wrapper.instrumentChromeApiFunction('permissions.contains', 1);
wrapper.instrumentChromeApiFunction('runtime.onInstalled.addListener', 0);
wrapper.instrumentChromeApiFunction('runtime.onStartup.addListener', 0);
wrapper.instrumentChromeApiFunction('storage.onChanged.addListener', 0);
wrapper.instrumentChromeApiFunction('tabs.create', 1);

var updateCardsAttempts = buildAttemptManager(
    'cards-update',
    requestCards,
    INITIAL_POLLING_PERIOD_SECONDS,
    MAXIMUM_POLLING_PERIOD_SECONDS);
var optInPollAttempts = buildAttemptManager(
    'optin',
    pollOptedInNoImmediateRecheck,
    INITIAL_POLLING_PERIOD_SECONDS,
    MAXIMUM_POLLING_PERIOD_SECONDS);
var optInRecheckAttempts = buildAttemptManager(
    'optin-recheck',
    pollOptedInWithRecheck,
    INITIAL_OPTIN_RECHECK_PERIOD_SECONDS,
    MAXIMUM_OPTIN_RECHECK_PERIOD_SECONDS);
var dismissalAttempts = buildAttemptManager(
    'dismiss',
    retryPendingDismissals,
    INITIAL_RETRY_DISMISS_PERIOD_SECONDS,
    MAXIMUM_RETRY_DISMISS_PERIOD_SECONDS);
var cardSet = buildCardSet();

var authenticationManager = buildAuthenticationManager();

/**
 * Google Now UMA event identifier.
 * @enum {number}
 */
var GoogleNowEvent = {
  REQUEST_FOR_CARDS_TOTAL: 0,
  REQUEST_FOR_CARDS_SUCCESS: 1,
  CARDS_PARSE_SUCCESS: 2,
  DISMISS_REQUEST_TOTAL: 3,
  DISMISS_REQUEST_SUCCESS: 4,
  LOCATION_REQUEST: 5,
  DELETED_LOCATION_UPDATE: 6,
  EXTENSION_START: 7,
  DELETED_SHOW_WELCOME_TOAST: 8,
  STOPPED: 9,
  DELETED_USER_SUPPRESSED: 10,
  SIGNED_OUT: 11,
  NOTIFICATION_DISABLED: 12,
  GOOGLE_NOW_DISABLED: 13,
  EVENTS_TOTAL: 14  // EVENTS_TOTAL is not an event; all new events need to be
                    // added before it.
};

/**
 * Records a Google Now Event.
 * @param {GoogleNowEvent} event Event identifier.
 */
function recordEvent(event) {
  var metricDescription = {
    metricName: 'GoogleNow.Event',
    type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
    min: 1,
    max: GoogleNowEvent.EVENTS_TOTAL,
    buckets: GoogleNowEvent.EVENTS_TOTAL + 1
  };

  chrome.metricsPrivate.recordValue(metricDescription, event);
}

/**
 * Records a notification clicked event.
 * @param {number|undefined} cardTypeId Card type ID.
 */
function recordNotificationClick(cardTypeId) {
  if (cardTypeId !== undefined) {
    chrome.metricsPrivate.recordSparseValue(
        'GoogleNow.Card.Clicked', cardTypeId);
  }
}

/**
 * Records a button clicked event.
 * @param {number|undefined} cardTypeId Card type ID.
 * @param {number} buttonIndex Button Index
 */
function recordButtonClick(cardTypeId, buttonIndex) {
  if (cardTypeId !== undefined) {
    chrome.metricsPrivate.recordSparseValue(
        'GoogleNow.Card.Button.Clicked' + buttonIndex, cardTypeId);
  }
}

/**
 * Checks the result of the HTTP Request and updates the authentication
 * manager on any failure.
 * @param {string} token Authentication token to validate against an
 *     XMLHttpRequest.
 * @return {function(XMLHttpRequest)} Function that validates the token with the
 *     supplied XMLHttpRequest.
 */
function checkAuthenticationStatus(token) {
  return function(request) {
    if (request.status == HTTP_FORBIDDEN ||
        request.status == HTTP_UNAUTHORIZED) {
      authenticationManager.removeToken(token);
    }
  }
}

/**
 * Builds and sends an authenticated request to the notification server.
 * @param {string} method Request method.
 * @param {string} handlerName Server handler to send the request to.
 * @param {string=} opt_contentType Value for the Content-type header.
 * @return {Promise} A promise to issue a request to the server.
 *     The promise rejects if the response is not within the HTTP 200 range.
 */
function requestFromServer(method, handlerName, opt_contentType) {
  return authenticationManager.getAuthToken().then(function(token) {
    var request = buildServerRequest(method, handlerName, opt_contentType);
    request.setRequestHeader('Authorization', 'Bearer ' + token);
    var requestPromise = new Promise(function(resolve, reject) {
      request.addEventListener('loadend', function() {
        if ((200 <= request.status) && (request.status < 300)) {
          resolve(request);
        } else {
          reject(request);
        }
      }, false);
      request.send();
    });
    requestPromise.catch(checkAuthenticationStatus(token));
    return requestPromise;
  });
}

/**
 * Shows the notification groups as notification cards.
 * @param {Object<StoredNotificationGroup>} notificationGroups Map from group
 *     name to group information.
 * @param {function(ReceivedNotification)=} opt_onCardShown Optional parameter
 *     called when each card is shown.
 * @return {Promise} A promise to show the notification groups as cards.
 */
function showNotificationGroups(notificationGroups, opt_onCardShown) {
  /** @type {Object<ChromeNotificationId, CombinedCard>} */
  var cards = combineCardsFromGroups(notificationGroups);
  console.log('showNotificationGroups ' + JSON.stringify(cards));

  return new Promise(function(resolve) {
    instrumented.notifications.getAll(function(notifications) {
      console.log('showNotificationGroups-getAll ' +
          JSON.stringify(notifications));
      notifications = notifications || {};

      // Mark notifications that didn't receive an update as having received
      // an empty update.
      for (var chromeNotificationId in notifications) {
        cards[chromeNotificationId] = cards[chromeNotificationId] || [];
      }

      /** @type {Object<ChromeNotificationId, NotificationDataEntry>} */
      var notificationsData = {};

      // Create/update/delete notifications.
      for (var chromeNotificationId in cards) {
        notificationsData[chromeNotificationId] = cardSet.update(
            chromeNotificationId,
            cards[chromeNotificationId],
            notificationGroups,
            opt_onCardShown);
      }
      chrome.storage.local.set({notificationsData: notificationsData});
      resolve();
    });
  });
}

/**
 * Removes all cards and card state on Google Now close down.
 */
function removeAllCards() {
  console.log('removeAllCards');

  // TODO(robliao): Once Google Now clears its own checkbox in the
  // notifications center and bug 260376 is fixed, the below clearing
  // code is no longer necessary.
  instrumented.notifications.getAll(function(notifications) {
    notifications = notifications || {};
    for (var chromeNotificationId in notifications) {
      instrumented.notifications.clear(chromeNotificationId, function() {});
    }
    chrome.storage.local.remove(['notificationsData', 'notificationGroups']);
  });
}

/**
 * Adds a card group into a set of combined cards.
 * @param {Object<ChromeNotificationId, CombinedCard>} combinedCards Map from
 *     chromeNotificationId to a combined card.
 *     This is an input/output parameter.
 * @param {StoredNotificationGroup} storedGroup Group to combine into the
 *     combined card set.
 */
function combineGroup(combinedCards, storedGroup) {
  for (var i = 0; i < storedGroup.cards.length; i++) {
    /** @type {ReceivedNotification} */
    var receivedNotification = storedGroup.cards[i];

    /** @type {UncombinedNotification} */
    var uncombinedNotification = {
      receivedNotification: receivedNotification,
      showTime: receivedNotification.trigger.showTimeSec &&
                (storedGroup.cardsTimestamp +
                 receivedNotification.trigger.showTimeSec * MS_IN_SECOND),
      hideTime: storedGroup.cardsTimestamp +
                receivedNotification.trigger.hideTimeSec * MS_IN_SECOND
    };

    var combinedCard =
        combinedCards[receivedNotification.chromeNotificationId] || [];
    combinedCard.push(uncombinedNotification);
    combinedCards[receivedNotification.chromeNotificationId] = combinedCard;
  }
}

/**
 * Calculates the soonest poll time from a map of groups as an absolute time.
 * @param {Object<StoredNotificationGroup>} groups Map from group name to group
 *     information.
 * @return {number} The next poll time based off of the groups.
 */
function calculateNextPollTimeMilliseconds(groups) {
  var nextPollTime = null;

  for (var groupName in groups) {
    var group = groups[groupName];
    if (group.nextPollTime !== undefined) {
      nextPollTime = nextPollTime == null ?
          group.nextPollTime : Math.min(group.nextPollTime, nextPollTime);
    }
  }

  // At least one of the groups must have nextPollTime.
  verify(nextPollTime != null, 'calculateNextPollTime: nextPollTime is null');
  return nextPollTime;
}

/**
 * Schedules next cards poll.
 * @param {Object<StoredNotificationGroup>} groups Map from group name to group
 *     information.
 */
function scheduleNextCardsPoll(groups) {
  var nextPollTimeMs = calculateNextPollTimeMilliseconds(groups);

  var nextPollDelaySeconds = Math.max(
      (nextPollTimeMs - Date.now()) / MS_IN_SECOND,
      MINIMUM_POLLING_PERIOD_SECONDS);
  updateCardsAttempts.start(nextPollDelaySeconds);
}

/**
 * Schedules the next opt-in check poll.
 */
function scheduleOptInCheckPoll() {
  instrumented.metricsPrivate.getVariationParams(
      'GoogleNow', function(params) {
    var optinPollPeriodSeconds =
        parseInt(params && params.optinPollPeriodSeconds, 10) ||
        DEFAULT_OPTIN_CHECK_PERIOD_SECONDS;
    optInPollAttempts.start(optinPollPeriodSeconds);
  });
}

/**
 * Combines notification groups into a set of Chrome notifications.
 * @param {Object<StoredNotificationGroup>} notificationGroups Map from group
 *     name to group information.
 * @return {Object<ChromeNotificationId, CombinedCard>} Cards to show.
 */
function combineCardsFromGroups(notificationGroups) {
  console.log('combineCardsFromGroups ' + JSON.stringify(notificationGroups));
  /** @type {Object<ChromeNotificationId, CombinedCard>} */
  var combinedCards = {};

  for (var groupName in notificationGroups)
    combineGroup(combinedCards, notificationGroups[groupName]);

  return combinedCards;
}

/**
 * Processes a server response for consumption by showNotificationGroups.
 * @param {ServerResponse} response Server response.
 * @return {Promise} A promise to process the server response and provide
 *     updated groups. Rejects if the server response shouldn't be processed.
 */
function processServerResponse(response) {
  console.log('processServerResponse ' + JSON.stringify(response));

  if (response.googleNowDisabled) {
    chrome.storage.local.set({googleNowEnabled: false});
    // Stop processing now. The state change will clear the cards.
    return Promise.reject();
  }

  var receivedGroups = response.groups;

  return fillFromChromeLocalStorage({
    /** @type {Object<StoredNotificationGroup>} */
    notificationGroups: {},
    /** @type {Object<ServerNotificationId, number>} */
    recentDismissals: {}
  }).then(function(items) {
    console.log('processServerResponse-get ' + JSON.stringify(items));

    // Build a set of non-expired recent dismissals. It will be used for
    // client-side filtering of cards.
    /** @type {Object<ServerNotificationId, number>} */
    var updatedRecentDismissals = {};
    var now = Date.now();
    for (var serverNotificationId in items.recentDismissals) {
      var dismissalAge = now - items.recentDismissals[serverNotificationId];
      if (dismissalAge < DISMISS_RETENTION_TIME_MS) {
        updatedRecentDismissals[serverNotificationId] =
            items.recentDismissals[serverNotificationId];
      }
    }

    // Populate groups with corresponding cards.
    if (response.notifications) {
      for (var i = 0; i < response.notifications.length; ++i) {
        /** @type {ReceivedNotification} */
        var card = response.notifications[i];
        if (!(card.notificationId in updatedRecentDismissals)) {
          var group = receivedGroups[card.groupName];
          group.cards = group.cards || [];
          group.cards.push(card);
        }
      }
    }

    // Build updated set of groups.
    var updatedGroups = {};

    for (var groupName in receivedGroups) {
      var receivedGroup = receivedGroups[groupName];
      var storedGroup = items.notificationGroups[groupName] || {
        cards: [],
        cardsTimestamp: undefined,
        nextPollTime: undefined,
        rank: undefined
      };

      if (receivedGroup.requested)
        receivedGroup.cards = receivedGroup.cards || [];

      if (receivedGroup.cards) {
        // If the group contains a cards update, all its fields will get new
        // values.
        storedGroup.cards = receivedGroup.cards;
        storedGroup.cardsTimestamp = now;
        storedGroup.rank = receivedGroup.rank;
        storedGroup.nextPollTime = undefined;
        // The code below assigns nextPollTime a defined value if
        // nextPollSeconds is specified in the received group.
        // If the group's cards are not updated, and nextPollSeconds is
        // unspecified, this method doesn't change group's nextPollTime.
      }

      // 'nextPollSeconds' may be sent even for groups that don't contain
      // cards updates.
      if (receivedGroup.nextPollSeconds !== undefined) {
        storedGroup.nextPollTime =
            now + receivedGroup.nextPollSeconds * MS_IN_SECOND;
      }

      updatedGroups[groupName] = storedGroup;
    }

    scheduleNextCardsPoll(updatedGroups);
    return {
      updatedGroups: updatedGroups,
      recentDismissals: updatedRecentDismissals
    };
  });
}

/**
 * Update the Explanatory Total Cards Shown Count.
 */
function countExplanatoryCard() {
  localStorage['explanatoryCardsShown']++;
}

/**
 * Determines if cards should have an explanation link.
 * @return {boolean} true if an explanatory card should be shown.
 */
function shouldShowExplanatoryCard() {
  var isBelowThreshold =
      localStorage['explanatoryCardsShown'] < EXPLANATORY_CARDS_LINK_THRESHOLD;
  return isBelowThreshold;
}

/**
 * Requests notification cards from the server for specified groups.
 * @param {Array<string>} groupNames Names of groups that need to be refreshed.
 * @return {Promise} A promise to request the specified notification groups.
 */
function requestNotificationGroupsFromServer(groupNames) {
  console.log(
      'requestNotificationGroupsFromServer from ' + NOTIFICATION_CARDS_URL +
      ', groupNames=' + JSON.stringify(groupNames));

  recordEvent(GoogleNowEvent.REQUEST_FOR_CARDS_TOTAL);

  var requestParameters = '?timeZoneOffsetMs=' +
    (-new Date().getTimezoneOffset() * MS_IN_MINUTE);

  if (shouldShowExplanatoryCard()) {
    requestParameters += '&cardExplanation=true';
  }

  groupNames.forEach(function(groupName) {
    requestParameters += ('&requestTypes=' + groupName);
  });

  requestParameters += '&uiLocale=' + navigator.language;

  console.log(
      'requestNotificationGroupsFromServer: request=' + requestParameters);

  return requestFromServer('GET', 'notifications' + requestParameters).then(
    function(request) {
      console.log(
          'requestNotificationGroupsFromServer-received ' + request.status);
      if (request.status == HTTP_OK) {
        recordEvent(GoogleNowEvent.REQUEST_FOR_CARDS_SUCCESS);
        return JSON.parse(request.responseText);
      }
    });
}

/**
 * Performs an opt-in poll without an immediate recheck.
 * If the response is not opted-in, schedule an opt-in check poll.
 */
function pollOptedInNoImmediateRecheck() {
  requestAndUpdateOptedIn()
      .then(function(optedIn) {
        if (!optedIn) {
          // Request a repoll if we're not opted in.
          return Promise.reject();
        }
      })
      .catch(function() {
        scheduleOptInCheckPoll();
      });
}

/**
 * Requests the account opted-in state from the server and updates any
 * state as necessary.
 * @return {Promise} A promise to request and update the opted-in state.
 *     The promise resolves with the opt-in state.
 */
function requestAndUpdateOptedIn() {
  console.log('requestOptedIn from ' + NOTIFICATION_CARDS_URL);

  return requestFromServer('GET', 'settings/optin').then(function(request) {
    console.log(
        'requestOptedIn-received ' + request.status + ' ' + request.response);
    if (request.status == HTTP_OK) {
      var parsedResponse = JSON.parse(request.responseText);
      return parsedResponse.value;
    }
  }).then(function(optedIn) {
    chrome.storage.local.set({googleNowEnabled: optedIn});
    return optedIn;
  });
}

/**
 * Determines the groups that need to be requested right now.
 * @return {Promise} A promise to determine the groups to request.
 */
function getGroupsToRequest() {
  return fillFromChromeLocalStorage({
    /** @type {Object<StoredNotificationGroup>} */
    notificationGroups: {}
  }).then(function(items) {
    console.log('getGroupsToRequest-storage-get ' + JSON.stringify(items));
    var groupsToRequest = [];
    var now = Date.now();

    for (var groupName in items.notificationGroups) {
      var group = items.notificationGroups[groupName];
      if (group.nextPollTime !== undefined && group.nextPollTime <= now)
        groupsToRequest.push(groupName);
    }
    return groupsToRequest;
  });
}

/**
 * Requests notification cards from the server.
 * @return {Promise} A promise to request the notification cards.
 *     Rejects if the cards won't be requested.
 */
function requestNotificationCards() {
  console.log('requestNotificationCards');
  return getGroupsToRequest()
      .then(requestNotificationGroupsFromServer)
      .then(processServerResponse)
      .then(function(processedResponse) {
        var onCardShown =
            shouldShowExplanatoryCard() ? countExplanatoryCard : undefined;
        return showNotificationGroups(
            processedResponse.updatedGroups, onCardShown).then(function() {
              chrome.storage.local.set({
                notificationGroups: processedResponse.updatedGroups,
                recentDismissals: processedResponse.updatedRecentDismissals
              });
              recordEvent(GoogleNowEvent.CARDS_PARSE_SUCCESS);
            }
          );
      });
}

/**
 * Determines if an immediate retry should occur based off of the given groups.
 * The NOR group is expected most often and less latency sensitive, so we will
 * simply wait MAXIMUM_POLLING_PERIOD_SECONDS before trying again.
 * @param {Array<string>} groupNames Names of groups that need to be refreshed.
 * @return {boolean} Whether a retry should occur.
 */
function shouldScheduleRetryFromGroupList(groupNames) {
  return (groupNames.length != 1) || (groupNames[0] !== 'NOR');
}

/**
 * Requests and shows notification cards.
 */
function requestCards() {
  console.log('requestCards @' + new Date());
  // LOCATION_REQUEST is a legacy histogram value when we requested location.
  // This corresponds to the extension attempting to request for cards.
  // We're keeping the name the same to keep our histograms in order.
  recordEvent(GoogleNowEvent.LOCATION_REQUEST);
  tasks.add(UPDATE_CARDS_TASK_NAME, function() {
    console.log('requestCards-task-begin');
    updateCardsAttempts.isRunning(function(running) {
      if (running) {
        // The cards are requested only if there are no unsent dismissals.
        processPendingDismissals()
            .then(requestNotificationCards)
            .catch(function() {
              return getGroupsToRequest().then(function(groupsToRequest) {
                if (shouldScheduleRetryFromGroupList(groupsToRequest)) {
                  updateCardsAttempts.scheduleRetry();
                }
              });
            });
      }
    });
  });
}

/**
 * Sends a server request to dismiss a card.
 * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID of
 *     the card.
 * @param {number} dismissalTimeMs Time of the user's dismissal of the card in
 *     milliseconds since epoch.
 * @param {DismissalData} dismissalData Data to build a dismissal request.
 * @return {Promise} A promise to request the card dismissal, rejects on error.
 */
function requestCardDismissal(
    chromeNotificationId, dismissalTimeMs, dismissalData) {
  console.log('requestDismissingCard ' + chromeNotificationId +
      ' from ' + NOTIFICATION_CARDS_URL +
      ', dismissalData=' + JSON.stringify(dismissalData));

  var dismissalAge = Date.now() - dismissalTimeMs;

  if (dismissalAge > MAXIMUM_DISMISSAL_AGE_MS) {
    return Promise.resolve();
  }

  recordEvent(GoogleNowEvent.DISMISS_REQUEST_TOTAL);

  var requestParameters = 'notifications/' + dismissalData.notificationId +
      '?age=' + dismissalAge +
      '&chromeNotificationId=' + chromeNotificationId;

  for (var paramField in dismissalData.parameters)
    requestParameters += ('&' + paramField +
    '=' + dismissalData.parameters[paramField]);

  console.log('requestCardDismissal: requestParameters=' + requestParameters);

  return requestFromServer('DELETE', requestParameters).then(function(request) {
    console.log('requestDismissingCard-onloadend ' + request.status);
    if (request.status == HTTP_NOCONTENT)
      recordEvent(GoogleNowEvent.DISMISS_REQUEST_SUCCESS);

    // A dismissal doesn't require further retries if it was successful or
    // doesn't have a chance for successful completion.
    return (request.status == HTTP_NOCONTENT) ?
           Promise.resolve() :
           Promise.reject();
  }).catch(function(request) {
    request = (typeof request === 'object') ? request : {};
    return (request.status == HTTP_BAD_REQUEST ||
           request.status == HTTP_METHOD_NOT_ALLOWED) ?
           Promise.resolve() :
           Promise.reject();
  });
}

/**
 * Tries to send dismiss requests for all pending dismissals.
 * @return {Promise} A promise to process the pending dismissals.
 *     The promise is rejected if a problem was encountered.
 */
function processPendingDismissals() {
  return fillFromChromeLocalStorage({
    /** @type {Array<PendingDismissal>} */
    pendingDismissals: [],
    /** @type {Object<ServerNotificationId, number>} */
    recentDismissals: {}
  }).then(function(items) {
    console.log(
        'processPendingDismissals-storage-get ' + JSON.stringify(items));

    var dismissalsChanged = false;

    function onFinish(success) {
      if (dismissalsChanged) {
        chrome.storage.local.set({
          pendingDismissals: items.pendingDismissals,
          recentDismissals: items.recentDismissals
        });
      }
      return success ? Promise.resolve() : Promise.reject();
    }

    function doProcessDismissals() {
      if (items.pendingDismissals.length == 0) {
        dismissalAttempts.stop();
        return onFinish(true);
      }

      // Send dismissal for the first card, and if successful, repeat
      // recursively with the rest.
      /** @type {PendingDismissal} */
      var dismissal = items.pendingDismissals[0];
      return requestCardDismissal(
          dismissal.chromeNotificationId,
          dismissal.time,
          dismissal.dismissalData).then(function() {
            dismissalsChanged = true;
            items.pendingDismissals.splice(0, 1);
            items.recentDismissals[dismissal.dismissalData.notificationId] =
                Date.now();
            return doProcessDismissals();
          }).catch(function() {
            return onFinish(false);
          });
    }

    return doProcessDismissals();
  });
}

/**
 * Submits a task to send pending dismissals.
 */
function retryPendingDismissals() {
  tasks.add(RETRY_DISMISS_TASK_NAME, function() {
    processPendingDismissals().catch(dismissalAttempts.scheduleRetry);
  });
}

/**
 * Opens a URL in a new tab.
 * @param {string} url URL to open.
 */
function openUrl(url) {
  instrumented.tabs.create({url: url}, function(tab) {
    if (tab)
      chrome.windows.update(tab.windowId, {focused: true});
    else
      chrome.windows.create({url: url, focused: true});
  });
}

/**
 * Opens URL corresponding to the clicked part of the notification.
 * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID of
 *     the card.
 * @param {function(NotificationDataEntry): (string|undefined)} selector
 *     Function that extracts the url for the clicked area from the
 *     notification data entry.
 */
function onNotificationClicked(chromeNotificationId, selector) {
  fillFromChromeLocalStorage({
    /** @type {Object<ChromeNotificationId, NotificationDataEntry>} */
    notificationsData: {}
  }).then(function(items) {
    /** @type {(NotificationDataEntry|undefined)} */
    var notificationDataEntry = items.notificationsData[chromeNotificationId];
    if (!notificationDataEntry)
      return;

    var url = selector(notificationDataEntry);
    if (!url)
      return;

    openUrl(url);
  });
}

/**
 * Callback for chrome.notifications.onClosed event.
 * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID of
 *     the card.
 * @param {boolean} byUser Whether the notification was closed by the user.
 */
function onNotificationClosed(chromeNotificationId, byUser) {
  if (!byUser)
    return;

  // At this point we are guaranteed that the notification is a now card.
  chrome.metricsPrivate.recordUserAction('GoogleNow.Dismissed');

  tasks.add(DISMISS_CARD_TASK_NAME, function() {
    dismissalAttempts.start();

    fillFromChromeLocalStorage({
      /** @type {Array<PendingDismissal>} */
      pendingDismissals: [],
      /** @type {Object<ChromeNotificationId, NotificationDataEntry>} */
      notificationsData: {},
      /** @type {Object<StoredNotificationGroup>} */
      notificationGroups: {}
    }).then(function(items) {
      /** @type {NotificationDataEntry} */
      var notificationData =
          items.notificationsData[chromeNotificationId] ||
          {
            timestamp: Date.now(),
            combinedCard: []
          };

      var dismissalResult =
          cardSet.onDismissal(
              chromeNotificationId,
              notificationData,
              items.notificationGroups);

      for (var i = 0; i < dismissalResult.dismissals.length; i++) {
        /** @type {PendingDismissal} */
        var dismissal = {
          chromeNotificationId: chromeNotificationId,
          time: Date.now(),
          dismissalData: dismissalResult.dismissals[i]
        };
        items.pendingDismissals.push(dismissal);
      }

      items.notificationsData[chromeNotificationId] =
          dismissalResult.notificationData;

      chrome.storage.local.set(items);

      processPendingDismissals();
    });
  });
}

/**
 * Initializes the polling system to start fetching cards.
 */
function startPollingCards() {
  console.log('startPollingCards');
  // Create an update timer for a case when for some reason requesting
  // cards gets stuck.
  updateCardsAttempts.start(MAXIMUM_POLLING_PERIOD_SECONDS);
  requestCards();
}

/**
 * Stops all machinery in the polling system.
 */
function stopPollingCards() {
  console.log('stopPollingCards');
  updateCardsAttempts.stop();
  // Since we're stopping everything, clear all runtime storage.
  // We don't clear localStorage since those values are still relevant
  // across Google Now start-stop events.
  chrome.storage.local.clear();
}

/**
 * Initializes the event page on install or on browser startup.
 */
function initialize() {
  recordEvent(GoogleNowEvent.EXTENSION_START);
  onStateChange();
}

/**
 * Starts or stops the main pipeline for polling cards.
 * @param {boolean} shouldPollCardsRequest true to start and
 *     false to stop polling cards.
 */
function setShouldPollCards(shouldPollCardsRequest) {
  updateCardsAttempts.isRunning(function(currentValue) {
    if (shouldPollCardsRequest != currentValue) {
      console.log('Action Taken setShouldPollCards=' + shouldPollCardsRequest);
      if (shouldPollCardsRequest)
        startPollingCards();
      else
        stopPollingCards();
    } else {
      console.log(
          'Action Ignored setShouldPollCards=' + shouldPollCardsRequest);
    }
  });
}

/**
 * Starts or stops the optin check and GCM channel to receive optin
 * notifications.
 * @param {boolean} shouldPollOptInStatus true to start and false to stop
 *     polling the optin status.
 */
function setShouldPollOptInStatus(shouldPollOptInStatus) {
  optInPollAttempts.isRunning(function(currentValue) {
    if (shouldPollOptInStatus != currentValue) {
      console.log(
          'Action Taken setShouldPollOptInStatus=' + shouldPollOptInStatus);
      if (shouldPollOptInStatus) {
        pollOptedInNoImmediateRecheck();
      } else {
        optInPollAttempts.stop();
      }
    } else {
      console.log(
          'Action Ignored setShouldPollOptInStatus=' + shouldPollOptInStatus);
    }
  });

  if (shouldPollOptInStatus) {
    registerForGcm();
  } else {
    unregisterFromGcm();
  }
}

/**
 * Enables or disables the Google Now background permission.
 * @param {boolean} backgroundEnable true to run in the background.
 *     false to not run in the background.
 */
function setBackgroundEnable(backgroundEnable) {
  instrumented.permissions.contains({permissions: ['background']},
      function(hasPermission) {
        if (backgroundEnable != hasPermission) {
          console.log('Action Taken setBackgroundEnable=' + backgroundEnable);
          if (backgroundEnable)
            chrome.permissions.request({permissions: ['background']});
          else
            chrome.permissions.remove({permissions: ['background']});
        } else {
          console.log('Action Ignored setBackgroundEnable=' + backgroundEnable);
        }
      });
}

/**
 * Record why this extension would not poll for cards.
 * @param {boolean} signedIn true if the user is signed in.
 * @param {boolean} notificationEnabled true if
 *     Google Now for Chrome is allowed to show notifications.
 * @param {boolean} googleNowEnabled true if
 *     the Google Now is enabled for the user.
 */
function recordEventIfNoCards(signedIn, notificationEnabled, googleNowEnabled) {
  if (!signedIn) {
    recordEvent(GoogleNowEvent.SIGNED_OUT);
  } else if (!notificationEnabled) {
    recordEvent(GoogleNowEvent.NOTIFICATION_DISABLED);
  } else if (!googleNowEnabled) {
    recordEvent(GoogleNowEvent.GOOGLE_NOW_DISABLED);
  }
}

/**
 * Does the actual work of deciding what Google Now should do
 * based off of the current state of Chrome.
 * @param {boolean} signedIn true if the user is signed in.
 * @param {boolean} canEnableBackground true if
 *     the background permission can be requested.
 * @param {boolean} notificationEnabled true if
 *     Google Now for Chrome is allowed to show notifications.
 * @param {boolean} googleNowEnabled true if
 *     the Google Now is enabled for the user.
 */
function updateRunningState(
    signedIn,
    canEnableBackground,
    notificationEnabled,
    googleNowEnabled) {
  console.log(
      'State Update signedIn=' + signedIn + ' ' +
      'canEnableBackground=' + canEnableBackground + ' ' +
      'notificationEnabled=' + notificationEnabled + ' ' +
      'googleNowEnabled=' + googleNowEnabled);

  var shouldPollCards = false;
  var shouldPollOptInStatus = false;
  var shouldSetBackground = false;

  if (signedIn && notificationEnabled) {
    shouldPollCards = googleNowEnabled;
    shouldPollOptInStatus = !googleNowEnabled;
    shouldSetBackground = canEnableBackground && googleNowEnabled;
  } else {
    recordEvent(GoogleNowEvent.STOPPED);
  }

  recordEventIfNoCards(signedIn, notificationEnabled, googleNowEnabled);

  console.log(
      'Requested Actions shouldSetBackground=' + shouldSetBackground + ' ' +
      'setShouldPollCards=' + shouldPollCards + ' ' +
      'shouldPollOptInStatus=' + shouldPollOptInStatus);

  setBackgroundEnable(shouldSetBackground);
  setShouldPollCards(shouldPollCards);
  setShouldPollOptInStatus(shouldPollOptInStatus);
  if (!shouldPollCards) {
    removeAllCards();
  }
}

/**
 * Coordinates the behavior of Google Now for Chrome depending on
 * Chrome and extension state.
 */
function onStateChange() {
  tasks.add(STATE_CHANGED_TASK_NAME, function() {
    Promise.all([
        authenticationManager.isSignedIn(),
        canEnableBackground(),
        isNotificationsEnabled(),
        isGoogleNowEnabled()])
        .then(function(results) {
          updateRunningState.apply(null, results);
        });
  });
}

/**
 * Determines if background mode should be requested.
 * @return {Promise} A promise to determine if background can be enabled.
 */
function canEnableBackground() {
  return new Promise(function(resolve) {
    instrumented.metricsPrivate.getVariationParams(
        'GoogleNow',
        function(response) {
          resolve(!response || (response.canEnableBackground != 'false'));
        });
  });
}

/**
 * Checks if Google Now is enabled in the notifications center.
 * @return {Promise} A promise to determine if Google Now is enabled
 *     in the notifications center.
 */
function isNotificationsEnabled() {
  return new Promise(function(resolve) {
    instrumented.notifications.getPermissionLevel(function(level) {
      resolve(level == 'granted');
    });
  });
}

/**
 * Gets the previous Google Now opt-in state.
 * @return {Promise} A promise to determine the previous Google Now
 *     opt-in state.
 */
function isGoogleNowEnabled() {
  return fillFromChromeLocalStorage({googleNowEnabled: false})
      .then(function(items) {
        return items.googleNowEnabled;
      });
}

/**
 * Ensures the extension is ready to listen for GCM messages.
 */
function registerForGcm() {
  // We don't need to use the key yet, just ensure the channel is set up.
  getGcmNotificationKey();
}

/**
 * Returns a Promise resolving to either a cached or new GCM notification key.
 * Rejects if registration fails.
 * @return {Promise} A Promise that resolves to a potentially-cached GCM key.
 */
function getGcmNotificationKey() {
  return fillFromChromeLocalStorage({gcmNotificationKey: undefined})
      .then(function(items) {
        if (items.gcmNotificationKey) {
          console.log('Reused gcm key from storage.');
          return Promise.resolve(items.gcmNotificationKey);
        }
        return requestNewGcmNotificationKey();
      });
}

/**
 * Returns a promise resolving to a GCM Notificaiton Key. May call
 * chrome.gcm.register() first if required. Rejects on registration failure.
 * @return {Promise} A Promise that resolves to a fresh GCM Notification key.
 */
function requestNewGcmNotificationKey() {
  return getGcmRegistrationId().then(function(gcmId) {
    authenticationManager.getAuthToken().then(function(token) {
      authenticationManager.getLogin().then(function(username) {
        return new Promise(function(resolve, reject) {
          var xhr = new XMLHttpRequest();
          xhr.responseType = 'application/json';
          xhr.open('POST', GCM_REGISTRATION_URL, true);
          xhr.setRequestHeader('Content-Type', 'application/json');
          xhr.setRequestHeader('Authorization', 'Bearer ' + token);
          xhr.setRequestHeader('project_id', GCM_PROJECT_ID);
          var payload = {
            'operation': 'add',
            'notification_key_name': username,
            'registration_ids': [gcmId]
          };
          xhr.onloadend = function() {
            if (xhr.status != 200) {
              reject();
            }
            var obj = JSON.parse(xhr.responseText);
            var key = obj && obj.notification_key;
            if (!key) {
              reject();
            }
            console.log('gcm notification key POST: ' + key);
            chrome.storage.local.set({gcmNotificationKey: key});
            resolve(key);
          };
          xhr.send(JSON.stringify(payload));
        });
      });
    }).catch(function() {
      // Couldn't obtain a GCM ID. Ignore and fallback to polling.
    });
  });
}

/**
 * Returns a promise resolving to either a cached or new GCM registration ID.
 * Rejects if registration fails.
 * @return {Promise} A Promise that resolves to a GCM registration ID.
 */
function getGcmRegistrationId() {
  return fillFromChromeLocalStorage({gcmRegistrationId: undefined})
      .then(function(items) {
        if (items.gcmRegistrationId) {
          console.log('Reused gcm registration id from storage.');
          return Promise.resolve(items.gcmRegistrationId);
        }

        return new Promise(function(resolve, reject) {
          instrumented.gcm.register([GCM_PROJECT_ID], function(registrationId) {
            console.log('gcm.register(): ' + registrationId);
            if (registrationId) {
              chrome.storage.local.set({gcmRegistrationId: registrationId});
              resolve(registrationId);
            } else {
              reject();
            }
          });
        });
      });
}

/**
 * Unregisters from GCM if previously registered.
 */
function unregisterFromGcm() {
  fillFromChromeLocalStorage({gcmRegistrationId: undefined})
      .then(function(items) {
        if (items.gcmRegistrationId) {
          console.log('Unregistering from gcm.');
          instrumented.gcm.unregister(function() {
            if (!chrome.runtime.lastError) {
              chrome.storage.local.remove(
                ['gcmNotificationKey', 'gcmRegistrationId']);
            }
          });
        }
      });
}

/**
 * Polls the optin state.
 * Sometimes we get the response to the opted in result too soon during
 * push messaging. We'll recheck the optin state a few times before giving up.
 */
function pollOptedInWithRecheck() {
  /**
   * Cleans up any state used to recheck the opt-in poll.
   */
  function clearPollingState() {
    localStorage.removeItem('optedInCheckCount');
    optInRecheckAttempts.stop();
  }

  if (localStorage.optedInCheckCount === undefined) {
    localStorage.optedInCheckCount = 0;
    optInRecheckAttempts.start();
  }

  console.log(new Date() +
      ' checkOptedIn Attempt ' + localStorage.optedInCheckCount);

  requestAndUpdateOptedIn().then(function(optedIn) {
    if (optedIn) {
      clearPollingState();
      return Promise.resolve();
    } else {
      // If we're not opted in, reject to retry.
      return Promise.reject();
    }
  }).catch(function() {
    if (localStorage.optedInCheckCount < 5) {
      localStorage.optedInCheckCount++;
      optInRecheckAttempts.scheduleRetry();
    } else {
      clearPollingState();
    }
  });
}

instrumented.runtime.onInstalled.addListener(function(details) {
  console.log('onInstalled ' + JSON.stringify(details));
  if (details.reason != 'chrome_update') {
    initialize();
  }
});

instrumented.runtime.onStartup.addListener(function() {
  console.log('onStartup');

  // Show notifications received by earlier polls. Doing this as early as
  // possible to reduce latency of showing first notifications. This mimics how
  // persistent notifications will work.
  tasks.add(SHOW_ON_START_TASK_NAME, function() {
    fillFromChromeLocalStorage({
      /** @type {Object<StoredNotificationGroup>} */
      notificationGroups: {}
    }).then(function(items) {
      console.log('onStartup-get ' + JSON.stringify(items));

      showNotificationGroups(items.notificationGroups).then(function() {
        chrome.storage.local.set(items);
      });
    });
  });

  initialize();
});

authenticationManager.addListener(function() {
  console.log('signIn State Change');
  onStateChange();
});

instrumented.notifications.onClicked.addListener(
    function(chromeNotificationId) {
      chrome.metricsPrivate.recordUserAction('GoogleNow.MessageClicked');
      onNotificationClicked(chromeNotificationId,
          function(notificationDataEntry) {
            var actionUrls = notificationDataEntry.actionUrls;
            var url = actionUrls && actionUrls.messageUrl;
            if (url) {
              recordNotificationClick(notificationDataEntry.cardTypeId);
            }
            return url;
          });
        });

instrumented.notifications.onButtonClicked.addListener(
    function(chromeNotificationId, buttonIndex) {
      chrome.metricsPrivate.recordUserAction(
          'GoogleNow.ButtonClicked' + buttonIndex);
      onNotificationClicked(chromeNotificationId,
          function(notificationDataEntry) {
            var actionUrls = notificationDataEntry.actionUrls;
            var url = actionUrls.buttonUrls[buttonIndex];
            if (url) {
              recordButtonClick(notificationDataEntry.cardTypeId, buttonIndex);
            } else {
              verify(false, 'onButtonClicked: no url for a button');
              console.log(
                  'buttonIndex=' + buttonIndex + ' ' +
                  'chromeNotificationId=' + chromeNotificationId + ' ' +
                  'notificationDataEntry=' +
                  JSON.stringify(notificationDataEntry));
            }
            return url;
          });
        });

instrumented.notifications.onClosed.addListener(onNotificationClosed);

instrumented.notifications.onPermissionLevelChanged.addListener(
    function(permissionLevel) {
      console.log('Notifications permissionLevel Change');
      onStateChange();
    });

instrumented.notifications.onShowSettings.addListener(function() {
  openUrl(SETTINGS_URL);
});

// Handles state change notifications for the Google Now enabled bit.
instrumented.storage.onChanged.addListener(function(changes, areaName) {
  if (areaName === 'local') {
    if ('googleNowEnabled' in changes) {
      onStateChange();
    }
  }
});

instrumented.gcm.onMessage.addListener(function(message) {
  console.log('gcm.onMessage ' + JSON.stringify(message));
  if (!message || !message.data) {
    return;
  }

  var payload = message.data.payload;
  var tag = message.data.tag;
  if (payload.startsWith('REQUEST_CARDS')) {
    tasks.add(ON_PUSH_MESSAGE_START_TASK_NAME, function() {
      // Accept promise rejection on failure since it's safer to do nothing,
      // preventing polling the server when the payload really didn't change.
      fillFromChromeLocalStorage({
        lastPollNowPayloads: {},
        /** @type {Object<StoredNotificationGroup>} */
        notificationGroups: {}
      }, PromiseRejection.ALLOW).then(function(items) {
        if (items.lastPollNowPayloads[tag] != payload) {
          items.lastPollNowPayloads[tag] = payload;

          items.notificationGroups['PUSH' + tag] = {
            cards: [],
            nextPollTime: Date.now()
          };

          chrome.storage.local.set({
            lastPollNowPayloads: items.lastPollNowPayloads,
            notificationGroups: items.notificationGroups
          });

          pollOptedInWithRecheck();
        }
      });
    });
  }
});
