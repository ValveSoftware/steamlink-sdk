// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Logic to process PDFs
 */

goog.provide('cvox.PdfProcessor');

/**
 * Process PDFs created with Chrome's built-in PDF plug-in, which has an
 * accessibility hook.
 */
cvox.PdfProcessor.processEmbeddedPdfs = function() {
  if (window.location.hash == '#original') {
    return;
  }

  var es = document.querySelectorAll('embed[type="application/pdf"]');
  for (var i = 0; i < es.length; i++) {
    var e = es[i];
    if (typeof e.accessibility === 'function') {
      var infoJSON = e.accessibility();
      var info = cvox.ChromeVoxJSON.parse(infoJSON);

      if (!info.loaded) {
        window.setTimeout(cvox.PdfProcessor.processEmbeddedPdfs, 100);
        continue;
      }
      if (!info.copyable) {
        cvox.ChromeVox.tts.speak(
            'Unable to access copy-protected PDF. Skipping.');
        continue;
      }

      var div = document.createElement('DIV');

      var headerDiv = document.createElement('DIV');
      headerDiv.style.position = 'relative';
      headerDiv.style.background = 'white';
      headerDiv.style.margin = '20pt';
      headerDiv.style.padding = '20pt';
      headerDiv.style.border = '1px solid #000';
      var filename = e.src.substr(e.src.lastIndexOf('/') + 1);
      document.title = filename;
      var html = cvox.ChromeVox.msgs.getMsg(
          'pdf_header', [filename, e.src + '#original']);
      headerDiv.innerHTML = html;
      // Set up a handler to reload the page when 'Show original' is clicked.
      var showLink = headerDiv.getElementsByTagName('a')[0];
      showLink.addEventListener('click', function() {
        window.location.href = e.src + '#original';
        window.location.reload();
      }, true);
      div.appendChild(headerDiv);

      // Document Styles
      div.style.position = 'relative';
      div.style.background = '#CCC';
      div.style.paddingTop = '1pt';
      div.style.paddingBottom = '1pt';
      div.style.width = '100%';
      div.style.minHeight = '100%';

      var displayPage = function(i) {
        var json = e.accessibility(i);
        var page = cvox.ChromeVoxJSON.parse(json);
        var pageDiv = document.createElement('DIV');
        var pageAnchor = document.createElement('A');

        // Page Achor Setup
        pageAnchor.name = 'page' + i;

        // Page Styles
        pageDiv.style.position = 'relative';
        pageDiv.style.background = 'white';
        pageDiv.style.margin = 'auto';
        pageDiv.style.marginTop = '20pt';
        pageDiv.style.marginBottom = '20pt';
        pageDiv.style.height = page.height + 'pt';
        pageDiv.style.width = page.width + 'pt';
        pageDiv.style.boxShadow = '0pt 0pt 10pt #333';

        // Text Nodes
        var texts = page['textBox'];
        for (var j = 0; j < texts.length; j++) {
          var text = texts[j];
          var textSpan = document.createElement('Span');

          // Text Styles
          textSpan.style.position = 'absolute';
          textSpan.style.left = text.left + 'pt';
          textSpan.style.top = text.top + 'pt';
          textSpan.style.fontSize = (0.8 * text.height) + 'pt';

          // Text Content
          for (var k = 0; k < text['textNodes'].length; k++) {
            var node = text['textNodes'][k];
            if (node.type == 'text') {
              textSpan.appendChild(document.createTextNode(node.text));
            } else if (node.type == 'url') {
              var a = document.createElement('A');
              a.href = node.url;
              a.appendChild(document.createTextNode(node.text));
              textSpan.appendChild(a);
            }
          }

          pageDiv.appendChild(textSpan);
        }
        div.appendChild(pageAnchor);
        div.appendChild(pageDiv);

        if (i < info['numberOfPages'] - 1) {
          window.setTimeout(function() { displayPage(i + 1); }, 0);
        } else {
          // NOTE(deboer): In the case of 'native' pdfs loaded directly
          // from a URL, we can not delete them outright.  Doing so exposes a
          // bug where all keyboard events are lost. We set it to
          // 'display: none' instead.
          e.style.display = 'none';
          e.parentNode.appendChild(div);

          // TODO(stoarca): Why are we resetting the navigationManager?
          // This function is too big and confusing, it needs to be cleaned up.
          cvox.ChromeVox.navigationManager.reset();
        }
      };

      window.setTimeout(function() { displayPage(0); }, 0);
    }
  }
};
