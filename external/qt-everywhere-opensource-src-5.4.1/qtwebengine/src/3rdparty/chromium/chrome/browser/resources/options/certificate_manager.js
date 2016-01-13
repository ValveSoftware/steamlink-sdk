// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // CertificateManagerTab class:

  /**
   * blah
   * @param {!string} id The id of this tab.
   * @param {boolean} isKiosk True if dialog is shown during CrOS kiosk launch.
   */
  function CertificateManagerTab(id, isKiosk) {
    this.tree = $(id + '-tree');

    options.CertificatesTree.decorate(this.tree);
    this.tree.addEventListener('change',
        this.handleCertificatesTreeChange_.bind(this));

    var tree = this.tree;

    this.viewButton = $(id + '-view');
    this.viewButton.onclick = function(e) {
      var selected = tree.selectedItem;
      chrome.send('viewCertificate', [selected.data.id]);
    }

    this.editButton = $(id + '-edit');
    if (this.editButton !== null) {
      if (id == 'serverCertsTab') {
        this.editButton.onclick = function(e) {
          var selected = tree.selectedItem;
          chrome.send('editServerCertificate', [selected.data.id]);
        }
      } else if (id == 'caCertsTab') {
        this.editButton.onclick = function(e) {
          var data = tree.selectedItem.data;
          CertificateEditCaTrustOverlay.show(data.id, data.name);
        }
      }
    }

    this.backupButton = $(id + '-backup');
    if (this.backupButton !== null) {
      if (id == 'personalCertsTab' && isKiosk) {
        this.backupButton.hidden = true;
      } else {
        this.backupButton.onclick = function(e) {
          var selected = tree.selectedItem;
          chrome.send('exportPersonalCertificate', [selected.data.id]);
        }
      }
    }

    this.backupAllButton = $(id + '-backup-all');
    if (this.backupAllButton !== null) {
      if (id == 'personalCertsTab' && isKiosk) {
        this.backupAllButton.hidden = true;
      } else {
        this.backupAllButton.onclick = function(e) {
          chrome.send('exportAllPersonalCertificates');
        }
      }
    }

    this.importButton = $(id + '-import');
    if (this.importButton !== null) {
      if (id == 'personalCertsTab') {
        if (isKiosk) {
          this.importButton.hidden = true;
        } else {
          this.importButton.onclick = function(e) {
            chrome.send('importPersonalCertificate', [false]);
          }
        }
      } else if (id == 'serverCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importServerCertificate');
        }
      } else if (id == 'caCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importCaCertificate');
        }
      }
    }

    this.importAndBindButton = $(id + '-import-and-bind');
    if (this.importAndBindButton !== null) {
      if (id == 'personalCertsTab') {
        this.importAndBindButton.onclick = function(e) {
          chrome.send('importPersonalCertificate', [true]);
        }
      }
    }

    this.exportButton = $(id + '-export');
    if (this.exportButton !== null) {
      if (id == 'personalCertsTab' && isKiosk) {
        this.exportButton.hidden = true;
      } else {
        this.exportButton.onclick = function(e) {
          var selected = tree.selectedItem;
          chrome.send('exportCertificate', [selected.data.id]);
        }
      }
    }

    this.deleteButton = $(id + '-delete');
    this.deleteButton.onclick = function(e) {
      var data = tree.selectedItem.data;
      AlertOverlay.show(
          loadTimeData.getStringF(id + 'DeleteConfirm', data.name),
          loadTimeData.getString(id + 'DeleteImpact'),
          loadTimeData.getString('ok'),
          loadTimeData.getString('cancel'),
          function() {
            tree.selectedItem = null;
            chrome.send('deleteCertificate', [data.id]);
          });
    }
  }

  CertificateManagerTab.prototype = {

    /**
     * Update button state.
     * @private
     * @param {!Object} data The data of the selected item.
     */
    updateButtonState: function(data) {
      var isCert = !!data && data.isCert;
      var readOnly = !!data && data.readonly;
      var extractable = !!data && data.extractable;
      var hasChildren = this.tree.items.length > 0;
      var isPolicy = !!data && data.policy;
      this.viewButton.disabled = !isCert;
      if (this.editButton !== null)
        this.editButton.disabled = !isCert || isPolicy;
      if (this.backupButton !== null)
        this.backupButton.disabled = !isCert || !extractable;
      if (this.backupAllButton !== null)
        this.backupAllButton.disabled = !hasChildren;
      if (this.exportButton !== null)
        this.exportButton.disabled = !isCert;
      this.deleteButton.disabled = !isCert || readOnly || isPolicy;
    },

    /**
     * Handles certificate tree selection change.
     * @private
     * @param {!Event} e The change event object.
     */
    handleCertificatesTreeChange_: function(e) {
      var data = null;
      if (this.tree.selectedItem) {
        data = this.tree.selectedItem.data;
      }

      this.updateButtonState(data);
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // CertificateManager class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function CertificateManager(model) {
    OptionsPage.call(this, 'certificates',
                     loadTimeData.getString('certificateManagerPageTabTitle'),
                     'certificateManagerPage');
  }

  cr.addSingletonGetter(CertificateManager);

  CertificateManager.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function(isKiosk) {
      OptionsPage.prototype.initializePage.call(this);

      this.personalTab = new CertificateManagerTab('personalCertsTab',
                                                   !!isKiosk);
      this.serverTab = new CertificateManagerTab('serverCertsTab', !!isKiosk);
      this.caTab = new CertificateManagerTab('caCertsTab', !!isKiosk);
      this.otherTab = new CertificateManagerTab('otherCertsTab', !!isKiosk);

      this.addEventListener('visibleChange', this.handleVisibleChange_);

      $('certificate-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };
    },

    initalized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (!this.initalized_ && this.visible) {
        this.initalized_ = true;
        OptionsPage.showTab($('personal-certs-nav-tab'));
        chrome.send('populateCertificateManager');
      }
    }
  };

  // CertificateManagerHandler callbacks.
  CertificateManager.onPopulateTree = function(args) {
    $(args[0]).populate(args[1]);
  };

  CertificateManager.exportPersonalAskPassword = function(args) {
    CertificateBackupOverlay.show();
  };

  CertificateManager.importPersonalAskPassword = function(args) {
    CertificateRestoreOverlay.show();
  };

  CertificateManager.onModelReady = function(userDbAvailable,
                                             tpmAvailable) {
    if (!userDbAvailable)
      return;
    if (tpmAvailable)
      $('personalCertsTab-import-and-bind').disabled = false;
    $('personalCertsTab-import').disabled = false;
    $('serverCertsTab-import').disabled = false;
    $('caCertsTab-import').disabled = false;
  };

  // Export
  return {
    CertificateManagerTab: CertificateManagerTab,
    CertificateManager: CertificateManager
  };
});
