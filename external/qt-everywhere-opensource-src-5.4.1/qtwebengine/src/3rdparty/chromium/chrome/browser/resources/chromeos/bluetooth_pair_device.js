// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var OptionsPage = options.OptionsPage;
var BluetoothPairing = options.BluetoothPairing;
var FakeBluetoothOverlayParent = options.FakeBluetoothOverlayParent;

/** @override */
OptionsPage.closeOverlay = function() {
  chrome.send('dialogClose');
};

/**
 * Listener for the |beforeunload| event.
 */
window.onbeforeunload = function() {
  OptionsPage.willClose();
};

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

  // TODO(ivankr): remove when http://crosbug.com/20660 is resolved.
  var inputs = document.querySelectorAll('input[pref]');
  for (var i = 0, el; el = inputs[i]; i++) {
    el.addEventListener('keyup', function(e) {
      cr.dispatchSimpleEvent(this, 'change');
    });
  }

  chrome.send('coreOptionsInitialize');

  OptionsPage.register(FakeBluetoothOverlayParent.getInstance());
  OptionsPage.registerOverlay(BluetoothPairing.getInstance(),
                              FakeBluetoothOverlayParent.getInstance());

  var device = {};
  var args = JSON.parse(chrome.getVariableValue('dialogArguments'));
  device = args;
  device.pairing = 'bluetoothStartConnecting';
  BluetoothPairing.showDialog(device);
  chrome.send('updateBluetoothDevice', [device.address, 'connect']);
}

document.addEventListener('DOMContentLoaded', load);
