// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AlertOverlay = options.AlertOverlay;
var OptionsPage = options.OptionsPage;
var CertificateManager = options.CertificateManager;
var CertificateRestoreOverlay = options.CertificateRestoreOverlay;
var CertificateBackupOverlay = options.CertificateBackupOverlay;
var CertificateEditCaTrustOverlay = options.CertificateEditCaTrustOverlay;
var CertificateImportErrorOverlay = options.CertificateImportErrorOverlay;

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  if (cr.isChromeOS)
    document.documentElement.setAttribute('os', 'chromeos');

  // Setup tab change handers.
  var subpagesNavTabs = document.querySelectorAll('.subpages-nav-tabs');
  for (var i = 0; i < subpagesNavTabs.length; i++) {
    subpagesNavTabs[i].onclick = function(event) {
      OptionsPage.showTab(event.srcElement);
    };
  }

  // Shake the dialog if the user clicks outside the dialog bounds.
  var containers = [$('overlay-container-2')];
  for (var i = 0; i < containers.length; i++) {
    var overlay = containers[i];
    cr.ui.overlay.setupOverlay(overlay);
    overlay.addEventListener('cancelOverlay',
                             OptionsPage.cancelOverlay.bind(OptionsPage));
  }

  // Hide elements that should not be part of the dialog.
  $('certificate-confirm').hidden = true;
  $('cert-manager-header').hidden = true;

  OptionsPage.isDialog = true;
  CertificateManager.getInstance().initializePage(true);
  OptionsPage.registerOverlay(AlertOverlay.getInstance(),
      CertificateManager.getInstance());
  OptionsPage.registerOverlay(CertificateBackupOverlay.getInstance(),
      CertificateManager.getInstance());
  OptionsPage.registerOverlay(CertificateEditCaTrustOverlay.getInstance(),
      CertificateManager.getInstance());
  OptionsPage.registerOverlay(CertificateImportErrorOverlay.getInstance(),
      CertificateManager.getInstance());
  OptionsPage.registerOverlay(CertificateManager.getInstance());
  OptionsPage.registerOverlay(CertificateRestoreOverlay.getInstance(),
      CertificateManager.getInstance());

  OptionsPage.showPageByName('certificates', false);
}

disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|url/.test(src.type);
});

document.addEventListener('DOMContentLoaded', load);
