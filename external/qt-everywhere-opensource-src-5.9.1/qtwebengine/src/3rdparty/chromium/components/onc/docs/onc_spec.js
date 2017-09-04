// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var outline_root = null;
var root = null;
var outline_ptr = null;

function onEnter(node) {
  var li = document.createElement('li');
  outline_ptr.appendChild(li);

  var header = node.querySelector('h1');
  header.id = 'sec_' + header.textContent.replace(/ /g, '_');
  var link = document.createElement('a');
  link.href = '#' + header.id;
  link.textContent = header.textContent;
  li.appendChild(link);
  var ul = document.createElement('ul');
  li.appendChild(ul);
  outline_ptr = ul;
}

function onExit(node) {
  outline_ptr = outline_ptr.parentNode.parentNode;
}

function outline(node) {
  var in_toc = !node.classList.contains('not_in_toc');
  if (in_toc) {
    onEnter(node);
  }
  var child = node.firstChild;
  while (child) {
    if (child.tagName === 'SECTION') {
      outline(child);
    }
    child = child.nextSibling;
  }
  if (in_toc) {
    onExit(node);
  }
}


window.onload = function () {
  outline_root = document.getElementById('outline');
  root = document.getElementById('root');

  var ul = document.createElement('ul');
  outline_root.appendChild(ul);
  outline_ptr = ul;

  outline(root);
};