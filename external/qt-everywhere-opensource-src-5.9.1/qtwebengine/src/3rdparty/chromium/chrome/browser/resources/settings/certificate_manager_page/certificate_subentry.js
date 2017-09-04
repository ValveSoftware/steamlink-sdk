// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-certificate-subentry represents an SSL certificate
 * sub-entry.
 */

cr.define('settings', function() {
  /**
   * The name of the event that is fired when a menu item is tapped.
   * @type {string}
   */
  var CertificateActionEvent = 'certificate-action';

  return {
    CertificateActionEvent: CertificateActionEvent,
  };
});

Polymer({
  is: 'settings-certificate-subentry',

  properties: {
    /** @type {!CertificateSubnode} */
    model: Object,

    /** @type {!CertificateType} */
    certificateType: String,
  },

  /** @private {settings.CertificatesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /**
   * Dispatches an event indicating which certificate action was tapped. It is
   * used by the parent of this element to display a modal dialog accordingly.
   * @param {!CertificateAction} action
   * @private
   */
  dispatchCertificateActionEvent_: function(action) {
    this.fire(
        settings.CertificateActionEvent,
        /** @type {!CertificateActionEventDetail} */ ({
          action: action,
          subnode: this.model,
          certificateType: this.certificateType,
        }));
  },

  /**
   * Handles the case where a call to the browser resulted in a rejected
   * promise.
   * @param {*} error Expects {?CertificatesError}.
   * @private
   */
  onRejected_: function(error) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // the native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.fire('certificates-error', error);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onViewTap_: function(event) {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEditTap_: function(event) {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.EDIT);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDeleteTap_: function(event) {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.DELETE);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onExportTap_: function(event) {
    this.closePopupMenu_();
    if (this.certificateType == CertificateType.PERSONAL) {
      this.browserProxy_.exportPersonalCertificate(this.model.id).then(
          function() {
            this.dispatchCertificateActionEvent_(
                CertificateAction.EXPORT_PERSONAL);
          }.bind(this),
          this.onRejected_.bind(this));
    } else {
      this.browserProxy_.exportCertificate(this.model.id);
    }
  },

  /**
   * @param {!CertificateType} certificateType
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be edited.
   * @private
   */
  canEdit_: function(certificateType, model) {
    return certificateType == CertificateType.CA && !model.policy;
  },

  /**
   * @param {!CertificateType} certificateType
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be exported.
   * @private
   */
  canExport_: function(certificateType, model) {
    if (certificateType == CertificateType.PERSONAL) {
      return model.extractable;
    }
    return true;
  },

  /**
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be deleted.
   * @private
   */
  canDelete_: function(model) {
    return !model.readonly && !model.policy;
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('dialog[is=cr-action-menu]').close();
  },

  /** @private */
  onDotsTap_: function() {
    var actionMenu = /** @type {!CrActionMenuElement} */(
        this.$.menu.get());
    actionMenu.showAt(assert(this.$$('paper-icon-button')));
  },
});
