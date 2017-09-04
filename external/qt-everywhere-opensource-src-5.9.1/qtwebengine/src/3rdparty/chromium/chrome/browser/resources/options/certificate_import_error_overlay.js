// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * CertificateImportErrorOverlay class
   * Displays a list of certificates and errors.
   * @class
   */
  function CertificateImportErrorOverlay() {
    Page.call(this, 'certificateImportErrorOverlay', '',
              'certificateImportErrorOverlay');
  }

  cr.addSingletonGetter(CertificateImportErrorOverlay);

  CertificateImportErrorOverlay.prototype = {
    // Inherit CertificateImportErrorOverlay from Page.
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('certificateImportErrorOverlayOk').onclick = function(event) {
        PageManager.closeOverlay();
      };
    },
  };

  /**
   * Show an alert overlay with the given message, button titles, and
   * callbacks.
   * @param {string} title The alert title to display to the user.
   * @param {string} message The alert message to display to the user.
   * @param {Array} certErrors The list of cert errors.  Each error should have
   *                           a .name and .error attribute.
   */
  CertificateImportErrorOverlay.show = function(title, message, certErrors) {
    $('certificateImportErrorOverlayTitle').textContent = title;
    $('certificateImportErrorOverlayMessage').textContent = message;

    var ul = $('certificateImportErrorOverlayCertErrors');
    ul.innerHTML = '';
    for (var i = 0; i < certErrors.length; ++i) {
      var li = document.createElement('li');
      li.textContent = loadTimeData.getStringF('certificateImportErrorFormat',
                                               certErrors[i].name,
                                               certErrors[i].error);
      ul.appendChild(li);
    }

    PageManager.showPageByName('certificateImportErrorOverlay');
  };

  // Export
  return {
    CertificateImportErrorOverlay: CertificateImportErrorOverlay
  };

});
