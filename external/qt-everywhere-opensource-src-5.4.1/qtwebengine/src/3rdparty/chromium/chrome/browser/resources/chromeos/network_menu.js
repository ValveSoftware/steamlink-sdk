// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Network status constants.
const StatusConnected = 'connected';
const StatusDisconnected = 'disconnected';
const StatusConnecting = 'connecting';
const StatusError = 'error';

const NetworkOther = 'other';

// Setup css canvas 'spinner-circle'
(function() {
  var lineWidth = 3;
  var r = 8;
  var ctx = document.getCSSCanvasContext('2d', 'spinner-circle', 2 * r, 2 * r);

  ctx.lineWidth = lineWidth;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';

  ctx.strokeStyle = '#4e73c7';
  ctx.beginPath();
  ctx.moveTo(lineWidth / 2, r - lineWidth / 2);
  ctx.arc(r, r, r - lineWidth / 2, Math.PI, Math.PI * 3 / 2);
  ctx.stroke();
})();

/**
 * Sends "connect" using the 'action' WebUI message.
 */
function sendConnect(index, passphrase, identity, auto_connect) {
  chrome.send('action',
      ['connect',
       String(index),
       passphrase,
       identity,
       auto_connect ? '1' : '0']);
}

var networkMenuItemProto = (function() {
    var networkMenuItem = cr.doc.createElement('div');
    networkMenuItem.innerHTML = '<div class="network-menu-item">' +
          '<div class="network-label-icon">' +
            '<div class="network-label"></div>' +
            '<div class="network-icon hidden"></div>' +
          '</div>' +
          '<div class="network-status hidden"></div>' +
          '<div class="hidden"></div>' +
        '</div>';
    return networkMenuItem;
  })();

var NetworkMenuItem = cr.ui.define(function() {
    return networkMenuItemProto.cloneNode(true);
  });

NetworkMenuItem.prototype = {
  __proto__: MenuItem.prototype,

  ssidEdit: null,
  passwordEdit: null,
  autoConnectCheckbox: null,

  /**
   * The label element.
   * @private
   */
  get label_() {
    return this.firstElementChild.firstElementChild.firstElementChild;
  },

  /**
   * The icon element.
   * @private
   */
  get icon_() {
    return this.label_.nextElementSibling;
  },

  /**
   * The status area element.
   * @private
   */
  get status_() {
    return this.firstElementChild.firstElementChild.nextElementSibling;
  },

  /**
   * The action area container element.
   * @private
   */
  get action_() {
    return this.status_.nextElementSibling;
  },

  /**
   * Set status message.
   * @param {string} message The message to display in status area.
   * @private
   */
  setStatus_: function(message) {
    if (message) {
      this.status_.textContent = message;
      this.status_.classList.remove('hidden');
    } else {
      this.status_.classList.add('hidden');
    }
  },

  /**
   * Set status icon.
   * @param {string} icon Source url for the icon image.
   * @private
   */
  setIcon_: function(icon) {
    if (icon) {
      this.icon_.style.backgroundImage = 'url(' + icon + ')';
      this.icon_.classList.remove('hidden');
    } else {
      this.icon_.classList.add('hidden');
    }
  },

  /**
   * Handle reconnect.
   * @private
   */
  handleConnect_ : function(e) {
    var index = this.menu_.getMenuItemIndexOf(this);
    if (this.ssidEdit && this.passwordEdit) {
      if (this.ssidEdit.value) {
        sendConnect(index,
            this.passwordEdit.value,
            this.ssidEdit.value,
            this.autoConnectCheckbox.checked);
      }
    } else if (this.passwordEdit) {
      if (this.passwordEdit.value) {
        sendConnect(index,
            this.passwordEdit.value, '', this.autoConnectCheckbox.checked);
      }
    } else {
      if (this.attrs.remembered) {
        sendConnect(index, this.attrs.passphrase, '', this.attrs.auto_connect);
      } else {
        sendConnect(index, '', '', this.autoConnectCheckbox.checked);
      }
    }
  },

  /**
   * Handle keydown event in ssid edit.
   * @private
   */
  handleSsidEditKeydown_: function(e) {
    if (e.target == this.ssidEdit &&
        e.keyIdentifier == 'Enter') {
      this.passwordEdit.focus();
    }
  },

  /**
   * Handle keydown event in password edit.
   * @private
   */
  handlePassEditKeydown_: function(e) {
    if (e.target == this.passwordEdit &&
        e.keyIdentifier == 'Enter') {
      this.handleConnect_();
    }
  },

  /**
   * Returns whether action area is visible.
   * @private
   */
  isActionVisible_: function() {
    return !this.action_.classList.contains('hidden');
  },

  /**
   * Show/hide action area.
   * @private
   */
  showAction_: function(show) {
    var visible = this.isActionVisible_();
    if (show && !visible) {
      this.action_.classList.remove('hidden');
    } else if (!show && visible) {
      this.action_.classList.add('hidden');
    }
  },

  /**
   * Add network name edit to action area.
   * @private
   */
  addSsidEdit_: function() {
    this.ssidEdit = this.ownerDocument.createElement('input');
    this.ssidEdit.type = 'text';
    this.ssidEdit.placeholder = localStrings.getString('ssid_prompt');
    this.ssidEdit.pattern = '^\\S+$';
    this.ssidEdit.addEventListener('keydown',
        this.handleSsidEditKeydown_.bind(this));

    var box = this.ownerDocument.createElement('div');
    box.appendChild(this.ssidEdit);
    this.action_.appendChild(box);
  },

  /**
   * Add password edit to action area.
   * @private
   */
  addPasswordEdit_: function() {
    this.passwordEdit = this.ownerDocument.createElement('input');
    this.passwordEdit.type = 'password';
    this.passwordEdit.placeholder = localStrings.getString('pass_prompt');
    this.passwordEdit.pattern = '^\\S+$';
    this.passwordEdit.addEventListener('keydown',
        this.handlePassEditKeydown_.bind(this));

    var box = this.ownerDocument.createElement('div');
    box.appendChild(this.passwordEdit);
    this.action_.appendChild(box);
  },

  /**
   * Add auto-connect this network check box to action area.
   * @private
   */
  addAutoConnectCheckbox_: function() {
    this.autoConnectCheckbox = this.ownerDocument.createElement('input');
    this.autoConnectCheckbox.type = 'checkbox';
    this.autoConnectCheckbox.checked = this.attrs.auto_connect;

    var autoConnectSpan = this.ownerDocument.createElement('span');
    autoConnectSpan.textContent =
        localStrings.getString('auto_connect_this_network');

    var autoConnectLabel = this.ownerDocument.createElement('label');
    autoConnectLabel.appendChild(this.autoConnectCheckbox);
    autoConnectLabel.appendChild(autoConnectSpan);

    this.action_.appendChild(autoConnectLabel);
  },

  /**
   * Internal method to initiailze the MenuItem.
   * @private
   */
  initMenuItem_: function() {
    // *TODO: eliminate code duplication with menu.js
    // MenuItem.prototype.initMenuItem_();
    var attrs = this.attrs;
    this.classList.add(attrs.type);
    this.menu_.addHandlers(this, this);

    //////// NetworkMenuItem specific code:
    // TODO: Handle specific types of network, connecting icon.
    this.label_.textContent = attrs.label;

    if (attrs.network_type == NetworkOther) {
      this.addSsidEdit_();
      this.addPasswordEdit_();
      this.addAutoConnectCheckbox_();
    } else if (attrs.status && attrs.status != 'unknown') {
      if (attrs.status == StatusConnected) {
        this.setStatus_(attrs.ip_address);
      } else if (attrs.status == StatusConnecting) {
        this.setStatus_(attrs.message);

        this.icon_.classList.add('spinner');
        this.icon_.classList.remove('hidden');
      } else if (attrs.status == StatusError) {
        this.setStatus_(attrs.message);
        this.setIcon_('chrome://theme/IDR_WARNING');

        var button = this.ownerDocument.createElement('button');
        button.textContent = localStrings.getString('reconnect');
        button.addEventListener('click', this.handleConnect_.bind(this));
        var box = this.ownerDocument.createElement('div');
        box.appendChild(button);
        this.action_.appendChild(box);

        this.showAction_(true);
      }

      if (attrs.need_passphrase) {
        this.addPasswordEdit_();
      }

      this.addAutoConnectCheckbox_();
    }
    //////// End NetworkMenuItem specifi code

    if (attrs.font) {
      this.label_.style.font = attrs.font;

      var baseFont = attrs.font.replace(/bold/, '').replace(/italic/, '');
      this.status_.style.font = baseFont;
      this.action_.style.font = baseFont;
    }
  },

  /** @override */
  activate: function() {
    // Close action area and connect if it is visible.
    if (this.isActionVisible_()) {
      this.showAction_(false);
      this.handleConnect_();
      return;
    }

    // Show action area for encrypted network and 'other' network.
    if ((this.attrs.network_type == NetworkOther ||
         this.attrs.status == StatusDisconnected) &&
        this.attrs.need_passphrase &&
        !this.isActionVisible_()) {
      this.showAction_(true);
      return;
    }

    MenuItem.prototype.activate.call(this);
  }
};


var NetworkMenu = cr.ui.define('div');

NetworkMenu.prototype = {
  __proto__: Menu.prototype,

  /** @override */
  createMenuItem: function(attrs) {
    if (attrs.type == 'command') {
      return new NetworkMenuItem();
    } else {
      return new MenuItem();
    }
  },

  /** @override */
  onClick_: function(event, item) {
    // If item is a NetworkMenuItem, it must have at least one of the following.
    if (item.autoConnectCheckbox || item.ssidEdit || item.passwordEdit) {
      // Ignore clicks other than on the NetworkMenuItem itself.
      if (event.target == item.autoConnectCheckbox ||
          event.target == item.autoConnectCheckbox.nextElementSibling ||
          event.target == item.ssidEdit ||
          event.target == item.passwordEdit) {
        return;
      }
    }

    Menu.prototype.onClick_.call(this, event, item);
  },
};
