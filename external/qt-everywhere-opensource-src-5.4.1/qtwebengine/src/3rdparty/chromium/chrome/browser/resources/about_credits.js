// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(o) {return document.getElementById(o);}

function toggle(o) {
  var licence = o.nextSibling;

  while (licence.className != 'licence') {
    if (!licence) return false;
    licence = licence.nextSibling;
  }

  if (licence.style && licence.style.display == 'block') {
    licence.style.display = 'none';
    o.innerHTML = 'show license';
  } else {
    licence.style.display = 'block';
    o.innerHTML = 'hide license';
  }
  return false;
}

document.body.onload = function () {
  var links = document.getElementsByTagName("a");
  for (var i = 0; i < links.length; i++) {
    if (links[i].className === "show") {
      links[i].onclick = function () { return toggle(this); };
    }
  }

  $("print-link").onclick = function () {
    window.print();
    return false;
  }
};

