// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function watchForTag(tagName, cb) {
  if (!document.body)
    return;

  function findChildTags(queryNode) {
    $Array.forEach(queryNode.querySelectorAll(tagName), function(node) {
      cb(node);
    });
  }
  // Query tags already in the document.
  findChildTags(document.body);

  // Observe the tags added later.
  var documentObserver = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, function(mutation) {
      $Array.forEach(mutation.addedNodes, function(addedNode) {
        if (addedNode.nodeType == Node.ELEMENT_NODE) {
          if (addedNode.tagName == tagName)
            cb(addedNode);
          findChildTags(addedNode);
        }
      });
    });
  });
  documentObserver.observe(document, {subtree: true, childList: true});
}

// Expose a function to watch the |tagName| introduction via mutation observer.
//
// We employee mutation observer to watch on any introduction of |tagName|
// within document so that we may handle it accordingly (either creating it or
// reporting error due to lack of permission).
// Think carefully about when to call this. On one hand, mutation observer
// functions on document, so we need to make sure document is finished
// parsing. To satisfy this, document.readyState has to be "interactive" or
// after. On the other hand, we intend to do this as early as possible so that
// developer would have no chance to bring in any conflicted property. To meet
// this requirement, we choose "readystatechange" event of window and use
// capturing way.
function addTagWatcher(tagName, cb) {
  var useCapture = true;
  window.addEventListener('readystatechange', function listener(event) {
    if (document.readyState == 'loading')
      return;

    watchForTag(tagName, cb);
    window.removeEventListener(event.type, listener, useCapture);
  }, useCapture);
}

exports.$set('addTagWatcher', addTagWatcher);
