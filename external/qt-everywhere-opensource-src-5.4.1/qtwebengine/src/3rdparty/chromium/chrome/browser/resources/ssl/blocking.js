// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function toggleMoreBox() {
  var helpBoxOuter = $('help-box-outer');
  helpBoxOuter.classList.toggle('hidden');
  var moreLessButton = $('more-less-button');
  if (helpBoxOuter.classList.contains('hidden')) {
    moreLessButton.innerText = templateData.more;
  } else {
    moreLessButton.innerText = templateData.less;
  }
}

function reloadPage() {
  sendCommand(CMD_RELOAD);
}

function setupEvents() {
  $('reload-button').addEventListener('click', reloadPage);
  $('more-less-button').addEventListener('click', toggleMoreBox);
}

document.addEventListener('DOMContentLoaded', setupEvents);
