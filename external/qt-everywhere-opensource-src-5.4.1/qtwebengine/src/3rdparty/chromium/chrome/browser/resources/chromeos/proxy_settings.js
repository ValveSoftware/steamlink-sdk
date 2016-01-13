// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var OptionsPage = options.OptionsPage;
var Preferences = options.Preferences;
var DetailsInternetPage = options.internet.DetailsInternetPage;

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  if (cr.isChromeOS)
    document.documentElement.setAttribute('os', 'chromeos');

  // Decorate the existing elements in the document.
  cr.ui.decorate('input[pref][type=checkbox]', options.PrefCheckbox);
  cr.ui.decorate('input[pref][type=number]', options.PrefNumber);
  cr.ui.decorate('input[pref][type=radio]', options.PrefRadio);
  cr.ui.decorate('input[pref][type=range]', options.PrefRange);
  cr.ui.decorate('select[pref]', options.PrefSelect);
  cr.ui.decorate('input[pref][type=text]', options.PrefTextField);
  cr.ui.decorate('input[pref][type=url]', options.PrefTextField);

  DetailsInternetPage.initializeProxySettings();

  // TODO(ivankr): remove when http://crosbug.com/20660 is resolved.
  var inputs = document.querySelectorAll('input[pref]');
  for (var i = 0, el; el = inputs[i]; i++) {
    el.addEventListener('keyup', function(e) {
      cr.dispatchSimpleEvent(this, 'change');
    });
  }

  Preferences.getInstance().initialize();
  chrome.send('coreOptionsInitialize');

  var params = parseQueryParams(window.location);
  var network = params.network;
  if (!network) {
    console.error('Error: No network argument provided!');
    network = '';
  }
  chrome.send('selectNetwork', [network]);

  DetailsInternetPage.showProxySettings();
}

disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|url/.test(src.type);
});

document.addEventListener('DOMContentLoaded', load);
