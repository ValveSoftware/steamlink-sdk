// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * HSTS is HTTPS Strict Transport Security: a way for sites to elect to always
 * use HTTPS. See http://dev.chromium.org/sts
 *
 * This UI allows a user to query and update the browser's list of HSTS domains.
 * It also allows users to query and update the browser's list of public key
 * pins.
 */

var HSTSView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HSTSView() {
    assertFirstConstructorCall(HSTSView);

    // Call superclass's constructor.
    superClass.call(this, HSTSView.MAIN_BOX_ID);

    this.addInput_ = $(HSTSView.ADD_INPUT_ID);
    this.addStsCheck_ = $(HSTSView.ADD_STS_CHECK_ID);
    this.addPkpCheck_ = $(HSTSView.ADD_PKP_CHECK_ID);
    this.addPins_ = $(HSTSView.ADD_PINS_ID);
    this.deleteInput_ = $(HSTSView.DELETE_INPUT_ID);
    this.queryInput_ = $(HSTSView.QUERY_INPUT_ID);
    this.queryOutputDiv_ = $(HSTSView.QUERY_OUTPUT_DIV_ID);

    var form = $(HSTSView.ADD_FORM_ID);
    form.addEventListener('submit', this.onSubmitAdd_.bind(this), false);

    form = $(HSTSView.DELETE_FORM_ID);
    form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

    form = $(HSTSView.QUERY_FORM_ID);
    form.addEventListener('submit', this.onSubmitQuery_.bind(this), false);

    g_browser.addHSTSObserver(this);
  }

  HSTSView.TAB_ID = 'tab-handle-hsts';
  HSTSView.TAB_NAME = 'HSTS';
  HSTSView.TAB_HASH = '#hsts';

  // IDs for special HTML elements in hsts_view.html
  HSTSView.MAIN_BOX_ID = 'hsts-view-tab-content';
  HSTSView.ADD_INPUT_ID = 'hsts-view-add-input';
  HSTSView.ADD_STS_CHECK_ID = 'hsts-view-check-sts-input';
  HSTSView.ADD_PKP_CHECK_ID = 'hsts-view-check-pkp-input';
  HSTSView.ADD_PINS_ID = 'hsts-view-add-pins';
  HSTSView.ADD_FORM_ID = 'hsts-view-add-form';
  HSTSView.ADD_SUBMIT_ID = 'hsts-view-add-submit';
  HSTSView.DELETE_INPUT_ID = 'hsts-view-delete-input';
  HSTSView.DELETE_FORM_ID = 'hsts-view-delete-form';
  HSTSView.DELETE_SUBMIT_ID = 'hsts-view-delete-submit';
  HSTSView.QUERY_INPUT_ID = 'hsts-view-query-input';
  HSTSView.QUERY_OUTPUT_DIV_ID = 'hsts-view-query-output';
  HSTSView.QUERY_FORM_ID = 'hsts-view-query-form';
  HSTSView.QUERY_SUBMIT_ID = 'hsts-view-query-submit';

  cr.addSingletonGetter(HSTSView);

  HSTSView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onSubmitAdd_: function(event) {
      g_browser.sendHSTSAdd(this.addInput_.value,
                            this.addStsCheck_.checked,
                            this.addPkpCheck_.checked,
                            this.addPins_.value);
      g_browser.sendHSTSQuery(this.addInput_.value);
      this.queryInput_.value = this.addInput_.value;
      this.addStsCheck_.checked = false;
      this.addPkpCheck_.checked = false;
      this.addInput_.value = '';
      this.addPins_.value = '';
      event.preventDefault();
    },

    onSubmitDelete_: function(event) {
      g_browser.sendHSTSDelete(this.deleteInput_.value);
      this.deleteInput_.value = '';
      event.preventDefault();
    },

    onSubmitQuery_: function(event) {
      g_browser.sendHSTSQuery(this.queryInput_.value);
      event.preventDefault();
    },

    onHSTSQueryResult: function(result) {
      if (result.error != undefined) {
        this.queryOutputDiv_.innerHTML = '';
        var s = addNode(this.queryOutputDiv_, 'span');
        s.textContent = result.error;
        s.style.color = '#e00';
        yellowFade(this.queryOutputDiv_);
        return;
      }

      if (result.result == false) {
        this.queryOutputDiv_.innerHTML = '<b>Not found</b>';
        yellowFade(this.queryOutputDiv_);
        return;
      }

      this.queryOutputDiv_.innerHTML = '';

      var s = addNode(this.queryOutputDiv_, 'span');
      s.innerHTML = '<b>Found:</b><br/>';

      var keys = [
        'domain', 'static_upgrade_mode', 'static_sts_include_subdomains',
        'static_pkp_include_subdomains', 'static_sts_observed',
        'static_pkp_observed', 'static_spki_hashes', 'dynamic_upgrade_mode',
        'dynamic_sts_include_subdomains', 'dynamic_pkp_include_subdomains',
        'dynamic_sts_observed', 'dynamic_pkp_observed', 'dynamic_spki_hashes'
      ];

      var kStaticHashKeys = [
        'public_key_hashes', 'preloaded_spki_hashes', 'static_spki_hashes'
      ];

      var staticHashes = [];
      for (var i = 0; i < kStaticHashKeys.length; ++i) {
        var staticHashValue = result[kStaticHashKeys[i]];
        if (staticHashValue != undefined && staticHashValue != '')
          staticHashes.push(staticHashValue);
      }

      for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        var value = result[key];
        addTextNode(this.queryOutputDiv_, ' ' + key + ': ');

        // If there are no static_hashes, do not make it seem like there is a
        // static PKP policy in place.
        if (staticHashes.length == 0 && key.indexOf('static_pkp_') == 0) {
          addNode(this.queryOutputDiv_, 'br');
          continue;
        }

        if (key === 'static_spki_hashes') {
          addNodeWithText(this.queryOutputDiv_, 'tt', staticHashes.join(','));
        } else if (key.indexOf('_upgrade_mode') >= 0) {
          addNodeWithText(this.queryOutputDiv_, 'tt', modeToString(value));
        } else {
          addNodeWithText(this.queryOutputDiv_, 'tt',
                          value == undefined ? '' : value);
        }
        addNode(this.queryOutputDiv_, 'br');
      }

      yellowFade(this.queryOutputDiv_);
    }
  };

  function modeToString(m) {
    // These numbers must match those in
    // TransportSecurityState::DomainState::UpgradeMode.
    if (m == 0) {
      return 'STRICT';
    } else if (m == 1) {
      return 'OPPORTUNISTIC';
    } else {
      return 'UNKNOWN';
    }
  }

  function yellowFade(element) {
    element.style.webkitTransitionProperty = 'background-color';
    element.style.webkitTransitionDuration = '0';
    element.style.backgroundColor = '#fffccf';
    setTimeout(function() {
      element.style.webkitTransitionDuration = '1000ms';
      element.style.backgroundColor = '#fff';
    }, 0);
  }

  return HSTSView;
})();
