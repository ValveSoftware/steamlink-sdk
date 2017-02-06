// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Show/hide trigger in a card.
 *
 * @typedef {{
 *   showTimeSec: (string|undefined),
 *   hideTimeSec: string
 * }}
 */
var Trigger;

/**
 * ID of an individual (uncombined) notification.
 * This ID comes directly from the server.
 *
 * @typedef {string}
 */
var ServerNotificationId;

/**
 * Data to build a dismissal request for a card from a specific group.
 *
 * @typedef {{
 *   notificationId: ServerNotificationId,
 *   parameters: Object
 * }}
 */
var DismissalData;

/**
 * Urls that need to be opened when clicking a notification or its buttons.
 *
 * @typedef {{
 *   messageUrl: (string|undefined),
 *   buttonUrls: (Array<string>|undefined)
 * }}
 */
var ActionUrls;

/**
 * ID of a combined notification.
 * This is the ID used with chrome.notifications API.
 *
 * @typedef {string}
 */
var ChromeNotificationId;

/**
 * Notification as sent by the server.
 *
 * @typedef {{
 *   notificationId: ServerNotificationId,
 *   chromeNotificationId: ChromeNotificationId,
 *   trigger: Trigger,
 *   chromeNotificationOptions: Object,
 *   actionUrls: (ActionUrls|undefined),
 *   dismissal: Object,
 *   locationBased: (boolean|undefined),
 *   groupName: string,
 *   cardTypeId: (number|undefined)
 * }}
 */
var ReceivedNotification;

/**
 * Received notification in a self-sufficient form that doesn't require group's
 * timestamp to calculate show and hide times.
 *
 * @typedef {{
 *   receivedNotification: ReceivedNotification,
 *   showTime: (number|undefined),
 *   hideTime: number
 * }}
 */
var UncombinedNotification;

/**
 * Card combined from potentially multiple groups.
 *
 * @typedef {Array<UncombinedNotification>}
 */
var CombinedCard;

/**
 * Data entry that we store for every Chrome notification.
 * |timestamp| is the time when corresponding Chrome notification was created or
 * updated last time by cardSet.update().
 *
 * @typedef {{
 *   actionUrls: (ActionUrls|undefined),
 *   cardTypeId: (number|undefined),
 *   timestamp: number,
 *   combinedCard: CombinedCard
 * }}
 *
 */
var NotificationDataEntry;

/**
 * Names for tasks that can be created by the this file.
 */
var UPDATE_CARD_TASK_NAME = 'update-card';

/**
 * Builds an object to manage notification card set.
 * @return {Object} Card set interface.
 */
function buildCardSet() {
  var alarmPrefix = 'card-';

  /**
   * Creates/updates/deletes a Chrome notification.
   * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID
   *     of the card.
   * @param {(ReceivedNotification|undefined)} receivedNotification Google Now
   *     card represented as a set of parameters for showing a Chrome
   *     notification, or null if the notification needs to be deleted.
   * @param {function(ReceivedNotification)=} onCardShown Optional parameter
   *     called when each card is shown.
   */
  function updateNotification(
      chromeNotificationId, receivedNotification, onCardShown) {
    console.log(
        'cardManager.updateNotification ' + chromeNotificationId + ' ' +
        JSON.stringify(receivedNotification));

    if (!receivedNotification) {
      instrumented.notifications.clear(chromeNotificationId, function() {});
      return;
    }

    // Try updating the notification.
    instrumented.notifications.update(
        chromeNotificationId,
        receivedNotification.chromeNotificationOptions,
        function(wasUpdated) {
          if (!wasUpdated) {
            // If the notification wasn't updated, it probably didn't exist.
            // Create it.
            console.log(
                'cardManager.updateNotification ' + chromeNotificationId +
                ' not updated, creating instead');
            instrumented.notifications.create(
                chromeNotificationId,
                receivedNotification.chromeNotificationOptions,
                function(newChromeNotificationId) {
                  if (!newChromeNotificationId || chrome.runtime.lastError) {
                    var errorMessage = chrome.runtime.lastError &&
                                       chrome.runtime.lastError.message;
                    console.error('notifications.create: ID=' +
                        newChromeNotificationId + ', ERROR=' + errorMessage);
                    return;
                  }

                  if (onCardShown !== undefined)
                    onCardShown(receivedNotification);
                });
          }
        });
  }

  /**
   * Iterates uncombined notifications in a combined card, determining for
   * each whether it's visible at the specified moment.
   * @param {CombinedCard} combinedCard The combined card in question.
   * @param {number} timestamp Time for which to calculate visibility.
   * @param {function(UncombinedNotification, boolean)} callback Function
   *     invoked for every uncombined notification in |combinedCard|.
   *     The boolean parameter indicates whether the uncombined notification is
   *     visible at |timestamp|.
   */
  function iterateUncombinedNotifications(combinedCard, timestamp, callback) {
    for (var i = 0; i != combinedCard.length; ++i) {
      var uncombinedNotification = combinedCard[i];
      var shouldShow = !uncombinedNotification.showTime ||
          uncombinedNotification.showTime <= timestamp;
      var shouldHide = uncombinedNotification.hideTime <= timestamp;

      callback(uncombinedNotification, shouldShow && !shouldHide);
    }
  }

  /**
   * Refreshes (shows/hides) the notification corresponding to the combined card
   * based on the current time and show-hide intervals in the combined card.
   * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID
   *     of the card.
   * @param {CombinedCard} combinedCard Combined cards with
   *     |chromeNotificationId|.
   * @param {Object<StoredNotificationGroup>} notificationGroups Map from group
   *     name to group information.
   * @param {function(ReceivedNotification)=} onCardShown Optional parameter
   *     called when each card is shown.
   * @return {(NotificationDataEntry|undefined)} Notification data entry for
   *     this card. It's 'undefined' if the card's life is over.
   */
  function update(
      chromeNotificationId, combinedCard, notificationGroups, onCardShown) {
    console.log('cardManager.update ' + JSON.stringify(combinedCard));

    chrome.alarms.clear(alarmPrefix + chromeNotificationId);
    var now = Date.now();
    /** @type {(UncombinedNotification|undefined)} */
    var winningCard = undefined;
    // Next moment of time when winning notification selection algotithm can
    // potentially return a different notification.
    /** @type {?number} */
    var nextEventTime = null;

    // Find a winning uncombined notification: a highest-priority notification
    // that needs to be shown now.
    iterateUncombinedNotifications(
        combinedCard,
        now,
        function(uncombinedCard, visible) {
          // If the uncombined notification is visible now and set the winning
          // card to it if its priority is higher.
          if (visible) {
            if (!winningCard ||
                uncombinedCard.receivedNotification.chromeNotificationOptions.
                    priority >
                winningCard.receivedNotification.chromeNotificationOptions.
                    priority) {
              winningCard = uncombinedCard;
            }
          }

          // Next event time is the closest hide or show event.
          if (uncombinedCard.showTime && uncombinedCard.showTime > now) {
            if (!nextEventTime || nextEventTime > uncombinedCard.showTime)
              nextEventTime = uncombinedCard.showTime;
          }
          if (uncombinedCard.hideTime > now) {
            if (!nextEventTime || nextEventTime > uncombinedCard.hideTime)
              nextEventTime = uncombinedCard.hideTime;
          }
        });

    // Show/hide the winning card.
    updateNotification(
        chromeNotificationId,
        winningCard && winningCard.receivedNotification,
        onCardShown);

    if (nextEventTime) {
      // If we expect more events, create an alarm for the next one.
      chrome.alarms.create(
          alarmPrefix + chromeNotificationId, {when: nextEventTime});

      // The trick with stringify/parse is to create a copy of action URLs,
      // otherwise notifications data with 2 pointers to the same object won't
      // be stored correctly to chrome.storage.
      var winningActionUrls = winningCard &&
          winningCard.receivedNotification.actionUrls &&
          JSON.parse(JSON.stringify(
              winningCard.receivedNotification.actionUrls));
      var winningCardTypeId = winningCard &&
          winningCard.receivedNotification.cardTypeId;
      return {
        actionUrls: winningActionUrls,
        cardTypeId: winningCardTypeId,
        timestamp: now,
        combinedCard: combinedCard
      };
    } else {
      // If there are no more events, we are done with this card. Note that all
      // received notifications have hideTime.
      verify(!winningCard, 'No events left, but card is shown.');
      clearCardFromGroups(chromeNotificationId, notificationGroups);
      return undefined;
    }
  }

  /**
   * Removes dismissed part of a card and refreshes the card. Returns remaining
   * dismissals for the combined card and updated notification data.
   * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID
   *     of the card.
   * @param {NotificationDataEntry} notificationData Stored notification entry
   *     for this card.
   * @param {Object<StoredNotificationGroup>} notificationGroups Map from group
   *     name to group information.
   * @return {{
   *   dismissals: Array<DismissalData>,
   *   notificationData: (NotificationDataEntry|undefined)
   * }}
   */
  function onDismissal(
      chromeNotificationId, notificationData, notificationGroups) {
    /** @type {Array<DismissalData>} */
    var dismissals = [];
    /** @type {Array<UncombinedNotification>} */
    var newCombinedCard = [];

    // Determine which parts of the combined card need to be dismissed or to be
    // preserved. We dismiss parts that were visible at the moment when the card
    // was last updated.
    iterateUncombinedNotifications(
      notificationData.combinedCard,
      notificationData.timestamp,
      function(uncombinedCard, visible) {
        if (visible) {
          dismissals.push({
            notificationId: uncombinedCard.receivedNotification.notificationId,
            parameters: uncombinedCard.receivedNotification.dismissal
          });
        } else {
          newCombinedCard.push(uncombinedCard);
        }
      });

    return {
      dismissals: dismissals,
      notificationData: update(
          chromeNotificationId, newCombinedCard, notificationGroups)
    };
  }

  /**
   * Removes card information from |notificationGroups|.
   * @param {ChromeNotificationId} chromeNotificationId chrome.notifications ID
   *     of the card.
   * @param {Object<StoredNotificationGroup>} notificationGroups Map from group
   *     name to group information.
   */
  function clearCardFromGroups(chromeNotificationId, notificationGroups) {
    console.log('cardManager.clearCardFromGroups ' + chromeNotificationId);
    for (var groupName in notificationGroups) {
      var group = notificationGroups[groupName];
      for (var i = 0; i != group.cards.length; ++i) {
        if (group.cards[i].chromeNotificationId == chromeNotificationId) {
          group.cards.splice(i, 1);
          break;
        }
      }
    }
  }

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    console.log('cardManager.onAlarm ' + JSON.stringify(alarm));

    if (alarm.name.indexOf(alarmPrefix) == 0) {
      // Alarm to show the card.
      tasks.add(UPDATE_CARD_TASK_NAME, function() {
        /** @type {ChromeNotificationId} */
        var chromeNotificationId = alarm.name.substring(alarmPrefix.length);
        fillFromChromeLocalStorage({
          /** @type {Object<ChromeNotificationId, NotificationDataEntry>} */
          notificationsData: {},
          /** @type {Object<StoredNotificationGroup>} */
          notificationGroups: {}
        }).then(function(items) {
          console.log('cardManager.onAlarm.get ' + JSON.stringify(items));

          var combinedCard =
            (items.notificationsData[chromeNotificationId] &&
             items.notificationsData[chromeNotificationId].combinedCard) || [];

          var cardShownCallback = undefined;
          if (localStorage['explanatoryCardsShown'] <
              EXPLANATORY_CARDS_LINK_THRESHOLD) {
             cardShownCallback = countExplanatoryCard;
          }

          items.notificationsData[chromeNotificationId] =
              update(
                  chromeNotificationId,
                  combinedCard,
                  items.notificationGroups,
                  cardShownCallback);

          chrome.storage.local.set(items);
        });
      });
    }
  });

  return {
    update: update,
    onDismissal: onDismissal
  };
}
