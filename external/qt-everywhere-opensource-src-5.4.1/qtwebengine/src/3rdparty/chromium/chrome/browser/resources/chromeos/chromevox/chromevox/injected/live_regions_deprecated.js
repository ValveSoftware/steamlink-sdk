// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of live regions on the page and speaks updates
 * when they change.
 *
 */

goog.provide('cvox.LiveRegionsDeprecated');

goog.require('cvox.AriaUtil');
goog.require('cvox.ChromeVox');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.NavDescription');

/**
 * @constructor
 */
cvox.LiveRegionsDeprecated = function() {
};

/**
 * An array of all of the elements on the page that are live regions.
 * @type {Array.<Element>}
 */
cvox.LiveRegionsDeprecated.trackedRegions = [];

/**
 * A parallel array to trackedRegions that stores the previous value of
 * each live region, represented as an array of NavDescriptions.
 * @type {Array.<Array.<cvox.NavDescription> >}
 */
cvox.LiveRegionsDeprecated.previousRegionValue = [];

/**
 * @type {Date}
 */
cvox.LiveRegionsDeprecated.pageLoadTime = null;

/**
 * Time in milliseconds after initial page load to ignore live region
 * updates, to avoid announcing regions as they're initially created.
 * The exception is alerts, they're announced when a page is loaded.
 * @type {number}
 * @const
 */
cvox.LiveRegionsDeprecated.INITIAL_SILENCE_MS = 2000;

/**
 * @param {Date} pageLoadTime The time the page was loaded. Live region
 *     updates within the first INITIAL_SILENCE_MS milliseconds are ignored.
 * @param {number} queueMode Interrupt or flush.  Polite live region
 *   changes always queue.
 * @param {boolean} disableSpeak true if change announcement should be disabled.
 * @return {boolean} true if any regions announced.
 */
cvox.LiveRegionsDeprecated.init = function(pageLoadTime, queueMode, disableSpeak) {
  if (queueMode == undefined) {
    queueMode = cvox.AbstractTts.QUEUE_MODE_FLUSH;
  }

  cvox.LiveRegionsDeprecated.pageLoadTime = pageLoadTime;

  var anyRegionsAnnounced = false;
  var regions = cvox.AriaUtil.getLiveRegions(document.body);
  for (var i = 0; i < regions.length; i++) {
    if (cvox.LiveRegionsDeprecated.updateLiveRegion(regions[i], queueMode,
                                          disableSpeak)) {
      anyRegionsAnnounced = true;
      queueMode = cvox.AbstractTts.QUEUE_MODE_QUEUE;
    }
  }

  return anyRegionsAnnounced;
};

/**
 * Speak relevant changes to a live region.
 *
 * @param {Node} region The live region node that changed.
 * @param {number} queueMode Interrupt or queue. Polite live region
 *   changes always queue.
 * @param {boolean} disableSpeak true if change announcement should be disabled.
 * @return {boolean} true if the region announced a change.
 */
cvox.LiveRegionsDeprecated.updateLiveRegion = function(region, queueMode, disableSpeak) {
  if (cvox.AriaUtil.getAriaBusy(region)) {
    return false;
  }

  // Make sure it's visible.
  if (!cvox.DomUtil.isVisible(region)) {
    return false;
  }

  // Retrieve the previous value of this region if we've tracked it
  // before, otherwise start tracking it.
  var regionIndex = cvox.LiveRegionsDeprecated.trackedRegions.indexOf(region);
  var previousValue;
  if (regionIndex >= 0) {
    previousValue = cvox.LiveRegionsDeprecated.previousRegionValue[regionIndex];
  } else {
    regionIndex = cvox.LiveRegionsDeprecated.trackedRegions.length;
    previousValue = [];
    cvox.LiveRegionsDeprecated.trackedRegions.push(region);
    cvox.LiveRegionsDeprecated.previousRegionValue.push([]);
  }

  // Get the new value.
  var currentValue = cvox.LiveRegionsDeprecated.buildCurrentLiveRegionValue(region);

  // If the page just loaded and this is any region type other than 'alert',
  // keep track of the new value but don't announce anything. Alerts are
  // the exception, they're announced on page load.
  var deltaTime = new Date() - cvox.LiveRegionsDeprecated.pageLoadTime;
  if (cvox.AriaUtil.getRoleAttribute(region) != 'alert' &&
      deltaTime < cvox.LiveRegionsDeprecated.INITIAL_SILENCE_MS) {
    cvox.LiveRegionsDeprecated.previousRegionValue[regionIndex] = currentValue;
    return false;
  }

  // Don't announce alerts on page load if their text and values consist of
  // just whitespace.
  if (cvox.AriaUtil.getRoleAttribute(region) == 'alert' &&
      deltaTime < cvox.LiveRegionsDeprecated.INITIAL_SILENCE_MS) {
    var regionText = '';
    for (var i = 0; i < currentValue.length; i++) {
      regionText += currentValue[i].text;
      regionText += currentValue[i].userValue;
    }
    if (cvox.DomUtil.collapseWhitespace(regionText) == '') {
      cvox.LiveRegionsDeprecated.previousRegionValue[regionIndex] = currentValue;
      return false;
    }
  }

  // Create maps of values in the live region for fast hash lookup.
  var previousValueMap = {};
  for (var i = 0; i < previousValue.length; i++) {
    previousValueMap[previousValue[i].toString()] = true;
  }
  var currentValueMap = {};
  for (i = 0; i < currentValue.length; i++) {
    currentValueMap[currentValue[i].toString()] = true;
  }

  // Figure out the additions and removals.
  var additions = [];
  if (cvox.AriaUtil.getAriaRelevant(region, 'additions')) {
    for (i = 0; i < currentValue.length; i++) {
      if (!previousValueMap[currentValue[i].toString()]) {
        additions.push(currentValue[i]);
      }
    }
  }
  var removals = [];
  if (cvox.AriaUtil.getAriaRelevant(region, 'removals')) {
    for (i = 0; i < previousValue.length; i++) {
      if (!currentValueMap[previousValue[i].toString()]) {
        removals.push(previousValue[i]);
      }
    }
  }

  // Only speak removals if they're the only change. Otherwise, when one or
  // more removals and additions happen concurrently, treat it as a change
  // and just speak any additions (which includes changed nodes).
  var messages = [];
  if (additions.length == 0 && removals.length > 0) {
    messages = [new cvox.NavDescription({
      context: cvox.ChromeVox.msgs.getMsg('live_regions_removed'), text: ''
    })].concat(removals);
  } else {
    messages = additions;
  }

  // Store the new value of the live region.
  cvox.LiveRegionsDeprecated.previousRegionValue[regionIndex] = currentValue;

  // Return if speak is disabled or there's nothing to announce.
  if (disableSpeak || messages.length == 0) {
    return false;
  }

  // Announce the changes with the appropriate politeness level.
  var live = cvox.AriaUtil.getAriaLive(region);
  if (live == 'polite') {
    queueMode = cvox.AbstractTts.QUEUE_MODE_QUEUE;
  }
  for (i = 0; i < messages.length; i++) {
    messages[i].speak(queueMode);
    queueMode = cvox.AbstractTts.QUEUE_MODE_QUEUE;
  }

  return true;
};

/**
 * Recursively build up the value of a live region and return it as
 * an array of NavDescriptions. Each atomic portion of the region gets a
 * single string, otherwise each leaf node gets its own string. When a region
 * changes, the sets of strings before and after are searched to determine
 * which have changed.
 *
 * @param {Node} node The root node.
 * @return {Array.<cvox.NavDescription>} An array of NavDescriptions
 *     describing atomic nodes or leaf nodes in the subtree rooted
 *     at this node.
 */
cvox.LiveRegionsDeprecated.buildCurrentLiveRegionValue = function(node) {
  if (cvox.AriaUtil.getAriaAtomic(node) ||
      cvox.DomUtil.isLeafNode(node)) {
    var description = cvox.DescriptionUtil.getDescriptionFromAncestors(
        [node], true, cvox.ChromeVox.verbosity);
    if (!description.isEmpty()) {
      return [description];
    } else {
      return [];
    }
  }

  var result = [];

  // Start with the description of this node.
  var description = cvox.DescriptionUtil.getDescriptionFromAncestors(
      [node], false, cvox.ChromeVox.verbosity);
  if (!description.isEmpty()) {
    result.push(description);
  }

  // Recursively add descriptions of child nodes.
  for (var i = 0; i < node.childNodes.length; i++) {
    var child = node.childNodes[i];
    if (cvox.DomUtil.isVisible(child, {checkAncestors: false}) &&
        !cvox.AriaUtil.isHidden(child)) {
      var recursiveArray = cvox.LiveRegionsDeprecated.buildCurrentLiveRegionValue(child);
      result = result.concat(recursiveArray);
    }
  }
  return result;
};
