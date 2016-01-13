// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('mobile', function() {

  function MobileSetup() {
  }

  cr.addSingletonGetter(MobileSetup);

  MobileSetup.PLAN_ACTIVATION_UNKNOWN = -2;
  MobileSetup.PLAN_ACTIVATION_PAGE_LOADING = -1;
  MobileSetup.PLAN_ACTIVATION_START = 0;
  MobileSetup.PLAN_ACTIVATION_TRYING_OTASP = 1;
  MobileSetup.PLAN_ACTIVATION_INITIATING_ACTIVATION = 3;
  MobileSetup.PLAN_ACTIVATION_RECONNECTING = 4;
  MobileSetup.PLAN_ACTIVATION_WAITING_FOR_CONNECTION = 5;
  MobileSetup.PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING = 6;
  MobileSetup.PLAN_ACTIVATION_SHOWING_PAYMENT = 7;
  MobileSetup.PLAN_ACTIVATION_RECONNECTING_PAYMENT = 8;
  MobileSetup.PLAN_ACTIVATION_DELAY_OTASP = 9;
  MobileSetup.PLAN_ACTIVATION_START_OTASP = 10;
  MobileSetup.PLAN_ACTIVATION_OTASP = 11;
  MobileSetup.PLAN_ACTIVATION_DONE = 12;
  MobileSetup.PLAN_ACTIVATION_ERROR = 0xFF;

  MobileSetup.EXTENSION_PAGE_URL =
      'chrome-extension://iadeocfgjdjdmpenejdbfeaocpbikmab';
  MobileSetup.ACTIVATION_PAGE_URL = MobileSetup.EXTENSION_PAGE_URL +
                                    '/activation.html';
  MobileSetup.PORTAL_OFFLINE_PAGE_URL = MobileSetup.EXTENSION_PAGE_URL +
                                        '/portal_offline.html';
  MobileSetup.REDIRECT_POST_PAGE_URL = MobileSetup.EXTENSION_PAGE_URL +
                                       '/redirect.html';

  MobileSetup.localStrings_ = new LocalStrings();

  MobileSetup.prototype = {
    // Mobile device information.
    deviceInfo_: null,
    frameName_: '',
    initialized_: false,
    fakedTransaction_: false,
    paymentShown_: false,
    frameLoadError_: 0,
    frameLoadIgnored_: true,
    carrierPageUrl_: null,
    spinnerInt_: -1,
    // UI states.
    state_: MobileSetup.PLAN_ACTIVATION_UNKNOWN,
    STATE_UNKNOWN_: 'unknown',
    STATE_CONNECTING_: 'connecting',
    STATE_ERROR_: 'error',
    STATE_PAYMENT_: 'payment',
    STATE_ACTIVATING_: 'activating',
    STATE_CONNECTED_: 'connected',

    initialize: function(frame_name, carrierPage) {
      if (this.initialized_) {
        console.log('calling initialize() again?');
        return;
      }
      this.initialized_ = true;
      self = this;
      this.frameName_ = frame_name;

      cr.ui.dialogs.BaseDialog.OK_LABEL =
        MobileSetup.localStrings_.getString('ok_button');
      cr.ui.dialogs.BaseDialog.CANCEL_LABEL =
          MobileSetup.localStrings_.getString('cancel_button');
      this.confirm_ = new cr.ui.dialogs.ConfirmDialog(document.body);

      window.addEventListener('message', function(e) {
          self.onMessageReceived_(e);
      });

      $('closeButton').addEventListener('click', function(e) {
        $('finalStatus').classList.add('hidden');
      });

      // Kick off activation process.
      chrome.send('startActivation');
    },

    startSpinner_: function() {
      this.stopSpinner_();
      this.spinnerInt_ = setInterval(mobile.MobileSetup.drawProgress, 100);
    },

    stopSpinner_: function() {
      if (this.spinnerInt_ != -1) {
        clearInterval(this.spinnerInt_);
        this.spinnerInt_ = -1;
      }
    },

    onFrameLoaded_: function(success) {
      chrome.send('paymentPortalLoad', [success ? 'ok' : 'failed']);
    },

    loadPaymentFrame_: function(deviceInfo) {
      if (deviceInfo) {
        this.frameLoadError_ = 0;
        this.deviceInfo_ = deviceInfo;
        if (deviceInfo.post_data && deviceInfo.post_data.length) {
          this.frameLoadIgnored_ = true;
          $(this.frameName_).contentWindow.location.href =
              MobileSetup.REDIRECT_POST_PAGE_URL +
              '?post_data=' + escape(deviceInfo.post_data) +
              '&formUrl=' + escape(deviceInfo.payment_url);
        } else {
          this.frameLoadIgnored_ = false;
          $(this.frameName_).contentWindow.location.href =
              deviceInfo.payment_url;
        }
      }
    },

    onMessageReceived_: function(e) {
      if (e.origin !=
              this.deviceInfo_.payment_url.substring(0, e.origin.length) &&
          e.origin != MobileSetup.EXTENSION_PAGE_URL)
        return;

      if (e.data.type == 'requestDeviceInfoMsg') {
        this.sendDeviceInfo_();
      } else if (e.data.type == 'framePostReady') {
        this.frameLoadIgnored_ = false;
        this.sendPostFrame_(e.origin);
      } else if (e.data.type == 'reportTransactionStatusMsg') {
        console.log('calling setTransactionStatus from onMessageReceived_');
        chrome.send('setTransactionStatus', [e.data.status]);
      }
    },

    changeState_: function(deviceInfo) {
      var newState = deviceInfo.state;
      if (this.state_ == newState)
        return;

      // The mobile setup is already in its final state.
      if (this.state_ == MobileSetup.PLAN_ACTIVATION_DONE ||
          this.state_ == MobileSetup.PLAN_ACTIVATION_ERROR) {
        return;
      }

      // Map handler state to UX.
      switch (newState) {
        case MobileSetup.PLAN_ACTIVATION_PAGE_LOADING:
        case MobileSetup.PLAN_ACTIVATION_START:
        case MobileSetup.PLAN_ACTIVATION_DELAY_OTASP:
        case MobileSetup.PLAN_ACTIVATION_START_OTASP:
        case MobileSetup.PLAN_ACTIVATION_RECONNECTING:
        case MobileSetup.PLAN_ACTIVATION_RECONNECTING_PAYMENT:
          // Activation page should not be shown for devices that are activated
          // over non cellular network.
          if (deviceInfo.activate_over_non_cellular_network)
            break;

          $('statusHeader').textContent =
              MobileSetup.localStrings_.getString('connecting_header');
          $('auxHeader').textContent =
              MobileSetup.localStrings_.getString('please_wait');
          $('paymentForm').classList.add('hidden');
          $('finalStatus').classList.add('hidden');
          this.setCarrierPage_(MobileSetup.ACTIVATION_PAGE_URL);
          $('systemStatus').classList.remove('hidden');
          $('canvas').classList.remove('hidden');
          this.startSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_TRYING_OTASP:
        case MobileSetup.PLAN_ACTIVATION_INITIATING_ACTIVATION:
        case MobileSetup.PLAN_ACTIVATION_OTASP:
          // Activation page should not be shown for devices that are activated
          // over non cellular network.
          if (deviceInfo.activate_over_non_cellular_network)
            break;

          $('statusHeader').textContent =
              MobileSetup.localStrings_.getString('activating_header');
          $('auxHeader').textContent =
              MobileSetup.localStrings_.getString('please_wait');
          $('paymentForm').classList.add('hidden');
          $('finalStatus').classList.add('hidden');
          this.setCarrierPage_(MobileSetup.ACTIVATION_PAGE_URL);
          $('systemStatus').classList.remove('hidden');
          $('canvas').classList.remove('hidden');
          this.startSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
          // Activation page should not be shown for devices that are activated
          // over non cellular network.
          if (!deviceInfo.activate_over_non_cellular_network) {
            $('statusHeader').textContent =
                MobileSetup.localStrings_.getString('connecting_header');
            $('auxHeader').textContent = '';
            $('paymentForm').classList.add('hidden');
            $('finalStatus').classList.add('hidden');
            this.setCarrierPage_(MobileSetup.ACTIVATION_PAGE_URL);
            $('systemStatus').classList.remove('hidden');
            $('canvas').classList.remove('hidden');
          }
          this.loadPaymentFrame_(deviceInfo);
          break;
        case MobileSetup.PLAN_ACTIVATION_WAITING_FOR_CONNECTION:
          $('statusHeader').textContent =
              MobileSetup.localStrings_.getString('portal_unreachable_header');
          $('auxHeader').textContent = '';
          $('auxHeader').classList.add('hidden');
          $('paymentForm').classList.add('hidden');
          $('finalStatus').classList.add('hidden');
          $('systemStatus').classList.remove('hidden');
          this.setCarrierPage_(MobileSetup.PORTAL_OFFLINE_PAGE_URL);
          $('canvas').classList.remove('hidden');
          this.startSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_SHOWING_PAYMENT:
          $('statusHeader').textContent = '';
          $('auxHeader').textContent = '';
          $('finalStatus').classList.add('hidden');
          $('systemStatus').classList.add('hidden');
          $('paymentForm').classList.remove('hidden');
          $('canvas').classList.add('hidden');
          this.stopSpinner_();
          this.paymentShown_ = true;
          break;
        case MobileSetup.PLAN_ACTIVATION_DONE:
          $('statusHeader').textContent = '';
          $('auxHeader').textContent = '';
          $('finalHeader').textContent =
              MobileSetup.localStrings_.getString('completed_header');
          $('finalMessage').textContent =
              MobileSetup.localStrings_.getString('completed_text');
          $('systemStatus').classList.add('hidden');
          $('closeButton').classList.remove('hidden');
          $('finalStatus').classList.remove('hidden');
          $('canvas').classList.add('hidden');
          $('closeButton').classList.toggle('hidden', !this.paymentShown_);
          $('paymentForm').classList.toggle('hidden', !this.paymentShown_);
          this.stopSpinner_();
          break;
        case MobileSetup.PLAN_ACTIVATION_ERROR:
          $('statusHeader').textContent = '';
          $('auxHeader').textContent = '';
          $('finalHeader').textContent =
              MobileSetup.localStrings_.getString('error_header');
          $('finalMessage').textContent = deviceInfo.error;
          $('systemStatus').classList.add('hidden');
          $('canvas').classList.add('hidden');
          $('closeButton').classList.toggle('hidden', !this.paymentShown_);
          $('paymentForm').classList.toggle('hidden', !this.paymentShown_);
          $('finalStatus').classList.remove('hidden');
          this.stopSpinner_();
          break;
      }
      this.state_ = newState;
    },

    setCarrierPage_: function(url) {
      if (this.carrierPageUrl_ == url)
        return;
      this.carrierPageUrl_ = url;
      $('carrierPage').contentWindow.location.href = url;
    },

    updateDeviceStatus_: function(deviceInfo) {
      this.changeState_(deviceInfo);
    },

    portalFrameLoadError_: function(errorCode) {
      if (this.frameLoadIgnored_)
        return;
      console.log('Portal frame load error detected: ', errorCode);
      this.frameLoadError_ = errorCode;
    },

    portalFrameLoadCompleted_: function() {
      if (this.frameLoadIgnored_)
        return;
      console.log('Portal frame load completed!');
      this.onFrameLoaded_(this.frameLoadError_ == 0);
    },

    sendPostFrame_: function(frameUrl) {
      var msg = { type: 'postFrame' };
      $(this.frameName_).contentWindow.postMessage(msg, frameUrl);
    },

    sendDeviceInfo_: function() {
      var msg = {
        type: 'deviceInfoMsg',
        domain: document.location,
        payload: {
          'carrier': this.deviceInfo_.carrier,
          'MEID': this.deviceInfo_.MEID,
          'IMEI': this.deviceInfo_.IMEI,
          'MDN': this.deviceInfo_.MDN
        }
      };
      $(this.frameName_).contentWindow.postMessage(msg,
          this.deviceInfo_.payment_url);
    }

  };

  MobileSetup.drawProgress = function() {
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    var segmentCount = Math.min(12, canvas.width / 1.6); // Number of segments
    var rotation = 0.75; // Counterclockwise rotation

    // Rotate canvas over time
    ctx.translate(canvas.width / 2, canvas.height / 2);
    ctx.rotate(Math.PI * 2 / (segmentCount + rotation));
    ctx.translate(-canvas.width / 2, -canvas.height / 2);

    var gap = canvas.width / 24; // Gap between segments
    var oRadius = canvas.width / 2; // Outer radius
    var iRadius = oRadius * 0.618; // Inner radius
    var oCircumference = Math.PI * 2 * oRadius; // Outer circumference
    var iCircumference = Math.PI * 2 * iRadius; // Inner circumference
    var oGap = gap / oCircumference; // Gap size as fraction of  outer ring
    var iGap = gap / iCircumference; // Gap size as fraction of  inner ring
    var oArc = Math.PI * 2 * (1 / segmentCount - oGap); // Angle of outer arcs
    var iArc = Math.PI * 2 * (1 / segmentCount - iGap); // Angle of inner arcs

    for (i = 0; i < segmentCount; i++) { // Draw each segment
      var opacity = Math.pow(1.0 - i / segmentCount, 3.0);
      opacity = (0.15 + opacity * 0.8); // Vary from 0.15 to 0.95
      var angle = - Math.PI * 2 * i / segmentCount;

      ctx.beginPath();
      ctx.arc(canvas.width / 2, canvas.height / 2, oRadius,
        angle - oArc / 2, angle + oArc / 2, false);
      ctx.arc(canvas.width / 2, canvas.height / 2, iRadius,
        angle + iArc / 2, angle - iArc / 2, true);
      ctx.closePath();
      ctx.fillStyle = 'rgba(240, 30, 29, ' + opacity + ')';
      ctx.fill();
    }
  };

  MobileSetup.deviceStateChanged = function(deviceInfo) {
    MobileSetup.getInstance().updateDeviceStatus_(deviceInfo);
  };

  MobileSetup.portalFrameLoadError = function(errorCode) {
    MobileSetup.getInstance().portalFrameLoadError_(errorCode);
  };

  MobileSetup.portalFrameLoadCompleted = function() {
    MobileSetup.getInstance().portalFrameLoadCompleted_();
  };

  MobileSetup.loadPage = function() {
    mobile.MobileSetup.getInstance().initialize('paymentForm',
        mobile.MobileSetup.ACTIVATION_PAGE_URL);
  };

  // Export
  return {
    MobileSetup: MobileSetup
  };
});

document.addEventListener('DOMContentLoaded', mobile.MobileSetup.loadPage);
