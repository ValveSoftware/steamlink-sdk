// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App launcher start page implementation.
 */

/**
 * The maximum height of the Google Doodle. Note this value should be consistent
 * with kWebViewHeight in start_page_view.cc.
 */
var doodleMaxHeight = 224;

cr.define('appList.startPage', function() {
  'use strict';

  // The element containing the current Google Doodle.
  var doodle = null;

  /**
   * Initialize the page.
   */
  function initialize() {
    chrome.send('initialize');
  }

  /**
   * Invoked when the app-list bubble is shown.
   */
  function onAppListShown() {
    chrome.send('appListShown', [this.doodle != null]);
  }

  /**
   * Sets the doodle's visibility, hiding or showing the default logo.
   *
   * @param {boolean} visible Whether the doodle should be made visible.
   */
  function setDoodleVisible(visible) {
    var doodle = $('doodle');
    var defaultLogo = $('default_logo');
    if (visible) {
      doodle.style.display = 'flex';
      defaultLogo.style.display = 'none';
    } else {
      if (doodle)
        doodle.style.display = 'none';

      defaultLogo.style.display = 'block';
    }
  }

  /**
   * Invoked when the app-list doodle is updated.
   *
   * @param {Object} data The data object representing the current doodle.
   */
  function onAppListDoodleUpdated(data, base_url) {
    if (this.doodle) {
      this.doodle.parentNode.removeChild(this.doodle);
      this.doodle = null;
    }

    var doodleData = data.ddljson;
    if (!doodleData || !doodleData.transparent_large_image) {
      setDoodleVisible(false);
      return;
    }

    // Set the page's base URL so that links will resolve relative to the Google
    // homepage.
    $('base').href = base_url;

    this.doodle = document.createElement('div');
    this.doodle.id = 'doodle';
    this.doodle.style.display = 'none';

    var doodleImage = document.createElement('img');
    doodleImage.id = 'doodle_image';
    if (doodleData.transparent_large_image.height > doodleMaxHeight)
      doodleImage.setAttribute('height', doodleMaxHeight);
    if (doodleData.alt_text) {
      doodleImage.alt = doodleData.alt_text;
      doodleImage.title = doodleData.alt_text;
    }

    doodleImage.onload = function() {
      setDoodleVisible(true);
    };
    doodleImage.src = doodleData.transparent_large_image.url;

    if (doodleData.target_url) {
      var doodleLink = document.createElement('a');
      doodleLink.id = 'doodle_link';
      doodleLink.href = doodleData.target_url;
      doodleLink.target = '_blank';
      doodleLink.appendChild(doodleImage);
      doodleLink.onclick = function() {
        chrome.send('doodleClicked');
        return true;
      };
      this.doodle.appendChild(doodleLink);
    } else {
      this.doodle.appendChild(doodleImage);
    }
    $('logo_container').appendChild(this.doodle);
  }

  return {
    initialize: initialize,
    onAppListDoodleUpdated: onAppListDoodleUpdated,
    onAppListShown: onAppListShown,
  };
});

document.addEventListener('contextmenu', function(e) { e.preventDefault(); });
document.addEventListener('DOMContentLoaded', appList.startPage.initialize);
