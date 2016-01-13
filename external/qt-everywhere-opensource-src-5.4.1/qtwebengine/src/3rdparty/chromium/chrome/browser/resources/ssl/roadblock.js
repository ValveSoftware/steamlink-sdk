// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UI modifications and event listeners that take place after load.
function setupEvents() {
  $('proceed-button').hidden = false;
  $('proceed-button').addEventListener('click', function() {
    sendCommand(CMD_PROCEED);
  });

  var details = document.querySelector('details.more');
  if ($('more-info-title').textContent == '') {
    details.hidden = true;
  } else {
    details.addEventListener('toggle', function handleToggle() {
      if (!details.open)
        return;
      sendCommand(CMD_MORE);
      details.removeEventListener('toggle', handleToggle);
    }, false);
  }

  $('exit-button').addEventListener('click', function() {
    sendCommand(CMD_DONT_PROCEED);
  });
}

document.addEventListener('DOMContentLoaded', setupEvents);
