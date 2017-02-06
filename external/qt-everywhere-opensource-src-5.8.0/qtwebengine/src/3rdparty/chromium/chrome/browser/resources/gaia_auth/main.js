// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Authenticator class wraps the communications between Gaia and its host.
 */
function Authenticator() {
}

/**
 * Gaia auth extension url origin.
 * @type {string}
 */
Authenticator.THIS_EXTENSION_ORIGIN =
    'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik';

/**
 * The lowest version of the credentials passing API supported.
 * @type {number}
 */
Authenticator.MIN_API_VERSION_VERSION = 1;

/**
 * The highest version of the credentials passing API supported.
 * @type {number}
 */
Authenticator.MAX_API_VERSION_VERSION = 1;

/**
 * The key types supported by the credentials passing API.
 * @type {Array} Array of strings.
 */
Authenticator.API_KEY_TYPES = [
  'KEY_TYPE_PASSWORD_PLAIN',
];

/**
 * Allowed origins of the hosting page.
 * @type {Array<string>}
 */
Authenticator.ALLOWED_PARENT_ORIGINS = [
  'chrome://oobe',
  'chrome://chrome-signin'
];

/**
 * Singleton getter of Authenticator.
 * @return {Object} The singleton instance of Authenticator.
 */
Authenticator.getInstance = function() {
  if (!Authenticator.instance_) {
    Authenticator.instance_ = new Authenticator();
  }
  return Authenticator.instance_;
};

Authenticator.prototype = {
  email_: null,
  gaiaId_: null,

  // Depending on the key type chosen, this will contain the plain text password
  // or a credential derived from it along with the information required to
  // repeat the derivation, such as a salt. The information will be encoded so
  // that it contains printable ASCII characters only. The exact encoding is TBD
  // when support for key types other than plain text password is added.
  passwordBytes_: null,

  needPassword_: false,
  chooseWhatToSync_: false,
  skipForNow_: false,
  sessionIndex_: null,
  attemptToken_: null,

  // Input params from extension initialization URL.
  inputLang_: undefined,
  intputEmail_: undefined,

  isSAMLFlow_: false,
  gaiaLoaded_: false,
  supportChannel_: null,

  useEafe_: false,
  clientId_: '',

  GAIA_URL: 'https://accounts.google.com/',
  GAIA_PAGE_PATH: 'ServiceLogin?skipvpage=true&sarp=1&rm=hide',
  SERVICE_ID: 'chromeoslogin',
  CONTINUE_URL: Authenticator.THIS_EXTENSION_ORIGIN + '/success.html',
  CONSTRAINED_FLOW_SOURCE: 'chrome',

  initialize: function() {
    var handleInitializeMessage = function(e) {
      if (Authenticator.ALLOWED_PARENT_ORIGINS.indexOf(e.origin) == -1) {
        console.error('Unexpected parent message, origin=' + e.origin);
        return;
      }
      window.removeEventListener('message', handleInitializeMessage);

      var params = e.data;
      params.parentPage = e.origin;
      this.initializeFromParent_(params);
      this.onPageLoad_();
    }.bind(this);

    document.addEventListener('DOMContentLoaded', function() {
      window.addEventListener('message', handleInitializeMessage);
      window.parent.postMessage({'method': 'loginUIDOMContentLoaded'}, '*');
    });
  },

  initializeFromParent_: function(params) {
    this.parentPage_ = params.parentPage;
    this.gaiaUrl_ = params.gaiaUrl || this.GAIA_URL;
    this.gaiaPath_ = params.gaiaPath || this.GAIA_PAGE_PATH;
    this.inputLang_ = params.hl;
    this.inputEmail_ = params.email;
    this.service_ = params.service || this.SERVICE_ID;
    this.continueUrl_ = params.continueUrl || this.CONTINUE_URL;
    this.desktopMode_ = params.desktopMode == '1';
    this.isConstrainedWindow_ = params.constrained == '1';
    this.useEafe_ = params.useEafe || false;
    this.clientId_ = params.clientId || '';
    this.initialFrameUrl_ = params.frameUrl || this.constructInitialFrameUrl_();
    this.initialFrameUrlWithoutParams_ = stripParams(this.initialFrameUrl_);
    this.needPassword_ = params.needPassword == '1';

    // For CrOS 'ServiceLogin' we assume that Gaia is loaded if we recieved
    // 'clearOldAttempts' message. For other scenarios Gaia doesn't send this
    // message so we have to rely on 'load' event.
    // TODO(dzhioev): Do not rely on 'load' event after b/16313327 is fixed.
    this.assumeLoadedOnLoadEvent_ =
        this.gaiaPath_.indexOf('ServiceLogin') !== 0 ||
        this.service_ !== 'chromeoslogin' ||
        this.useEafe_;
  },

  isGaiaMessage_: function(msg) {
    // Not quite right, but good enough.
    return this.gaiaUrl_.indexOf(msg.origin) == 0 ||
           this.GAIA_URL.indexOf(msg.origin) == 0;
  },

  isParentMessage_: function(msg) {
    return msg.origin == this.parentPage_;
  },

  constructInitialFrameUrl_: function() {
    var url = this.gaiaUrl_ + this.gaiaPath_;

    url = appendParam(url, 'service', this.service_);
    // Easy bootstrap use auth_code message as success signal instead of
    // continue URL.
    if (!this.useEafe_)
      url = appendParam(url, 'continue', this.continueUrl_);
    if (this.inputLang_)
      url = appendParam(url, 'hl', this.inputLang_);
    if (this.inputEmail_)
      url = appendParam(url, 'Email', this.inputEmail_);
    if (this.isConstrainedWindow_)
      url = appendParam(url, 'source', this.CONSTRAINED_FLOW_SOURCE);
    return url;
  },

  onPageLoad_: function() {
    window.addEventListener('message', this.onMessage.bind(this), false);
    this.initSupportChannel_();

    if (this.assumeLoadedOnLoadEvent_) {
      var gaiaFrame = $('gaia-frame');
      var handler = function() {
        gaiaFrame.removeEventListener('load', handler);
        if (!this.gaiaLoaded_) {
          this.gaiaLoaded_ = true;
          this.maybeInitialized_();

          if (this.useEafe_ && this.clientId_) {
            // Sends initial handshake message to EAFE. Note this fails with
            // SSO redirect because |gaiaFrame| sits on a different origin.
            gaiaFrame.contentWindow.postMessage({
              clientId: this.clientId_
            }, this.gaiaUrl_);
          }
        }
      }.bind(this);
      gaiaFrame.addEventListener('load', handler);
    }
  },

  initSupportChannel_: function() {
    var supportChannel = new Channel();
    supportChannel.connect('authMain');

    supportChannel.registerMessage('channelConnected', function() {
      // Load the gaia frame after the background page indicates that it is
      // ready, so that the webRequest handlers are all setup first.
      var gaiaFrame = $('gaia-frame');
      gaiaFrame.src = this.initialFrameUrl_;

      if (this.supportChannel_) {
        console.error('Support channel is already initialized.');
        return;
      }
      this.supportChannel_ = supportChannel;

      if (this.desktopMode_) {
        this.supportChannel_.send({
          name: 'initDesktopFlow',
          gaiaUrl: this.gaiaUrl_,
          continueUrl: stripParams(this.continueUrl_),
          isConstrainedWindow: this.isConstrainedWindow_,
          initialFrameUrlWithoutParams: this.initialFrameUrlWithoutParams_
        });

        this.supportChannel_.registerMessage(
            'switchToFullTab', this.switchToFullTab_.bind(this));
      }
      this.supportChannel_.registerMessage(
          'completeLogin', this.onCompleteLogin_.bind(this));
      this.initSAML_();
      this.supportChannel_.send({name: 'resetAuth'});
      this.maybeInitialized_();
    }.bind(this));

    window.setTimeout(function() {
      if (!this.supportChannel_) {
        // Give up previous channel and bind its 'channelConnected' to a no-op.
        supportChannel.registerMessage('channelConnected', function() {});

        // Re-initialize the channel if it is not connected properly, e.g.
        // connect may be called before background script started running.
        this.initSupportChannel_();
      }
    }.bind(this), 200);
  },

  /**
   * Called when one of the initialization stages has finished. If all the
   * needed parts are initialized, notifies parent about successfull
   * initialization.
   */
  maybeInitialized_: function() {
    if (!this.gaiaLoaded_ || !this.supportChannel_)
      return;
    var msg = {
      'method': 'loginUILoaded'
    };
    window.parent.postMessage(msg, this.parentPage_);
  },

  /**
   * Invoked when the background script sends a message to indicate that the
   * current content does not fit in a constrained window.
   * @param {Object=} msg Extra info to send.
   */
  switchToFullTab_: function(msg) {
    var parentMsg = {
      'method': 'switchToFullTab',
      'url': msg.url
    };
    window.parent.postMessage(parentMsg, this.parentPage_);
  },

  /**
   * Invoked when the signin flow is complete.
   * @param {Object=} opt_extraMsg Optional extra info to send.
   */
  completeLogin_: function(opt_extraMsg) {
    var msg = {
      'method': 'completeLogin',
      'email': (opt_extraMsg && opt_extraMsg.email) || this.email_,
      'password': this.passwordBytes_ ||
                  (opt_extraMsg && opt_extraMsg.password),
      'usingSAML': this.isSAMLFlow_,
      'chooseWhatToSync': this.chooseWhatToSync_ || false,
      'skipForNow': (opt_extraMsg && opt_extraMsg.skipForNow) ||
                    this.skipForNow_,
      'sessionIndex': (opt_extraMsg && opt_extraMsg.sessionIndex) ||
                      this.sessionIndex_,
      'gaiaId': (opt_extraMsg && opt_extraMsg.gaiaId) || this.gaiaId_
    };
    window.parent.postMessage(msg, this.parentPage_);
    this.supportChannel_.send({name: 'resetAuth'});
  },

  /**
   * Invoked when support channel is connected.
   */
  initSAML_: function() {
    this.isSAMLFlow_ = false;

    this.supportChannel_.registerMessage(
        'onAuthPageLoaded', this.onAuthPageLoaded_.bind(this));
    this.supportChannel_.registerMessage(
        'onInsecureContentBlocked', this.onInsecureContentBlocked_.bind(this));
    this.supportChannel_.registerMessage(
        'apiCall', this.onAPICall_.bind(this));
    this.supportChannel_.send({
      name: 'setGaiaUrl',
      gaiaUrl: this.gaiaUrl_
    });
    if (!this.desktopMode_ && this.gaiaUrl_.indexOf('https://') == 0) {
      // Abort the login flow when content served over an unencrypted connection
      // is detected on Chrome OS. This does not apply to tests that explicitly
      // set a non-https GAIA URL and want to perform all authentication over
      // http.
      this.supportChannel_.send({
        name: 'setBlockInsecureContent',
        blockInsecureContent: true
      });
    }
  },

  /**
   * Invoked when the background page sends 'onHostedPageLoaded' message.
   * @param {!Object} msg Details sent with the message.
   */
  onAuthPageLoaded_: function(msg) {
    if (msg.isSAMLPage && !this.isSAMLFlow_) {
      // GAIA redirected to a SAML login page. The credentials provided to this
      // page will determine what user gets logged in. The credentials obtained
      // from the GAIA login form are no longer relevant and can be discarded.
      this.isSAMLFlow_ = true;
      this.email_ = null;
      this.gaiaId_ = null;
      this.passwordBytes_ = null;
    }

    window.parent.postMessage({
      'method': 'authPageLoaded',
      'isSAML': this.isSAMLFlow_,
      'domain': extractDomain(msg.url)
    }, this.parentPage_);
  },

  /**
   * Invoked when the background page sends an 'onInsecureContentBlocked'
   * message.
   * @param {!Object} msg Details sent with the message.
   */
  onInsecureContentBlocked_: function(msg) {
    window.parent.postMessage({
      'method': 'insecureContentBlocked',
      'url': stripParams(msg.url)
    }, this.parentPage_);
  },

  /**
   * Invoked when one of the credential passing API methods is called by a SAML
   * provider.
   * @param {!Object} msg Details of the API call.
   */
  onAPICall_: function(msg) {
    var call = msg.call;
    if (call.method == 'initialize') {
      if (!Number.isInteger(call.requestedVersion) ||
          call.requestedVersion < Authenticator.MIN_API_VERSION_VERSION) {
        this.sendInitializationFailure_();
        return;
      }

      this.apiVersion_ = Math.min(call.requestedVersion,
                                  Authenticator.MAX_API_VERSION_VERSION);
      this.initialized_ = true;
      this.sendInitializationSuccess_();
      return;
    }

    if (call.method == 'add') {
      if (Authenticator.API_KEY_TYPES.indexOf(call.keyType) == -1) {
        console.error('Authenticator.onAPICall_: unsupported key type');
        return;
      }
      // Not setting |email_| and |gaiaId_| because this API call will
      // eventually be followed by onCompleteLogin_() which does set it.
      this.apiToken_ = call.token;
      this.passwordBytes_ = call.passwordBytes;
    } else if (call.method == 'confirm') {
      if (call.token != this.apiToken_)
        console.error('Authenticator.onAPICall_: token mismatch');
    } else {
      console.error('Authenticator.onAPICall_: unknown message');
    }
  },

  onGotAuthCode_: function(authCode) {
    window.parent.postMessage({
      'method': 'completeAuthenticationAuthCodeOnly',
      'authCode': authCode
    }, this.parentPage_);
  },

  sendInitializationSuccess_: function() {
    this.supportChannel_.send({name: 'apiResponse', response: {
      result: 'initialized',
      version: this.apiVersion_,
      keyTypes: Authenticator.API_KEY_TYPES
    }});
  },

  sendInitializationFailure_: function() {
    this.supportChannel_.send({
      name: 'apiResponse',
      response: {result: 'initialization_failed'}
    });
  },

  /**
   * Callback invoked for 'completeLogin' message.
   * @param {Object=} msg Message sent from background page.
   */
  onCompleteLogin_: function(msg) {
    if (!msg.email || !msg.gaiaId || !msg.sessionIndex) {
      // On desktop, if the skipForNow message field is set, send it to handler.
      // This does not require the email, gaiaid or session to be valid.
      if (this.desktopMode_ && msg.skipForNow) {
        this.completeLogin_(msg);
      } else {
        console.error('Missing fields to complete login.');
        window.parent.postMessage({method: 'missingGaiaInfo'},
                                  this.parentPage_);
        return;
      }
    }

    // Skip SAML extra steps for desktop flow and non-SAML flow.
    if (!this.isSAMLFlow_ || this.desktopMode_) {
      this.completeLogin_(msg);
      return;
    }

    this.email_ = msg.email;
    this.gaiaId_ = msg.gaiaId;
    // Password from |msg| is not used because ChromeOS SAML flow
    // gets password by asking user to confirm.
    this.skipForNow_ = msg.skipForNow;
    this.sessionIndex_ = msg.sessionIndex;

    if (this.passwordBytes_) {
      // If the credentials passing API was used, login is complete.
      window.parent.postMessage({method: 'samlApiUsed'}, this.parentPage_);
      this.completeLogin_(msg);
    } else if (!this.needPassword_) {
      // If the credentials passing API was not used, the password was obtained
      // by scraping. It must be verified before use. However, the host may not
      // be interested in the password at all. In that case, verification is
      // unnecessary and login is complete.
      this.completeLogin_(msg);
    } else {
      this.supportChannel_.sendWithCallback(
          {name: 'getScrapedPasswords'},
          function(passwords) {
            if (passwords.length == 0) {
              window.parent.postMessage(
                  {method: 'noPassword', email: this.email_},
                  this.parentPage_);
            } else {
              window.parent.postMessage({method: 'confirmPassword',
                                         email: this.email_,
                                         passwordCount: passwords.length},
                                        this.parentPage_);
            }
          }.bind(this));
    }
  },

  onVerifyConfirmedPassword_: function(password) {
    this.supportChannel_.sendWithCallback(
        {name: 'getScrapedPasswords'},
        function(passwords) {
          for (var i = 0; i < passwords.length; ++i) {
            if (passwords[i] == password) {
              this.passwordBytes_ = passwords[i];
              // SAML login is complete when the user has successfully
              // confirmed the password.
              if (this.passwordBytes_ !== null)
                this.completeLogin_();
              return;
            }
          }
          window.parent.postMessage(
              {method: 'confirmPassword', email: this.email_},
              this.parentPage_);
        }.bind(this));
  },

  onMessage: function(e) {
    var msg = e.data;

    if (this.useEafe_) {
      if (msg == '!_{h:\'gaia-frame\'}' && this.isGaiaMessage_(e)) {
        // Sends client ID again on the hello message to work around the SSO
        // signin issue.
        // TODO(xiyuan): Revisit this when EAFE is integrated or for webview.
        $('gaia-frame').contentWindow.postMessage({
          clientId: this.clientId_
        }, this.gaiaUrl_);
      } else if (typeof msg == 'object' &&
                 msg.type == 'authorizationCode' && this.isGaiaMessage_(e)) {
        this.onGotAuthCode_(msg.authorizationCode);
      } else {
        console.error('Authenticator.onMessage: unknown message' +
                      ', msg=' + JSON.stringify(msg));
      }

      return;
    }

    if (msg.method == 'attemptLogin' && this.isGaiaMessage_(e)) {
      // At this point GAIA does not yet know the gaiaId, so its not set here.
      this.email_ = msg.email;
      this.passwordBytes_ = msg.password;
      this.attemptToken_ = msg.attemptToken;
      this.chooseWhatToSync_ = msg.chooseWhatToSync;
      this.isSAMLFlow_ = false;
      if (this.supportChannel_)
        this.supportChannel_.send({name: 'startAuth'});
      else
        console.error('Support channel is not initialized.');
    } else if (msg.method == 'clearOldAttempts' && this.isGaiaMessage_(e)) {
      if (!this.gaiaLoaded_) {
        this.gaiaLoaded_ = true;
        this.maybeInitialized_();
      }
      this.email_ = null;
      this.gaiaId_ = null;
      this.sessionIndex_ = false;
      this.passwordBytes_ = null;
      this.attemptToken_ = null;
      this.isSAMLFlow_ = false;
      this.skipForNow_ = false;
      this.chooseWhatToSync_ = false;
      if (this.supportChannel_) {
        this.supportChannel_.send({name: 'resetAuth'});
        // This message is for clearing saml properties in gaia_auth_host and
        // oobe_screen_oauth_enrollment.
        window.parent.postMessage({
          'method': 'resetAuthFlow',
        }, this.parentPage_);
      }
    } else if (msg.method == 'verifyConfirmedPassword' &&
               this.isParentMessage_(e)) {
      this.onVerifyConfirmedPassword_(msg.password);
    } else if (msg.method == 'redirectToSignin' &&
               this.isParentMessage_(e)) {
      $('gaia-frame').src = this.constructInitialFrameUrl_();
    } else {
      console.error('Authenticator.onMessage: unknown message + origin!?');
    }
  }
};

Authenticator.getInstance().initialize();
