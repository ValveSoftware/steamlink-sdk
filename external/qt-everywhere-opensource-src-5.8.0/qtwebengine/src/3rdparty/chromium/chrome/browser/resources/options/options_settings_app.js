// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  if (document.location != 'chrome://settings-frame/options_settings_app.html')
    return;

  OptionsPage.setIsSettingsApp(true);

  // Override the offset in the options page.
  PageManager.horizontalOffset = 38;

  document.addEventListener('DOMContentLoaded', function() {
    // Hide everything by default.
    var sections = document.querySelectorAll('section');
    for (var i = 0; i < sections.length; i++)
      sections[i].hidden = true;

    var whitelistedSections = [
      'advanced-settings',
      'downloads-section',
      'handlers-section',
      'languages-section',
      'network-section',
      'notifications-section',
      'sync-section'
    ];

    for (var i = 0; i < whitelistedSections.length; i++)
      $(whitelistedSections[i]).hidden = false;

    // Avoid showing an empty Users section on ash. Note that profiles-section
    // is actually a div element, rather than section, so is not hidden after
    // the querySelectorAll(), above.
    $('sync-users-section').hidden = $('profiles-section').hidden;

    // Hide Import bookmarks and settings button.
    $('import-data').hidden = true;

    // Hide create / edit / delete profile buttons.
    $('profiles-create').hidden = true;
    $('profiles-delete').hidden = true;
    $('profiles-manage').hidden = true;

    // Remove the 'X'es on profiles in the profile list.
    $('profiles-list').canDeleteItems = false;
  });

  loadTimeData.overrideValues(/** @type {!Object} */(
      loadTimeData.getValue('settingsApp')));
}());
