// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a list for showing kiosk apps.
   * @constructor
   * @extends {cr.ui.List}
   */
  var KioskAppList = cr.ui.define('list');

  KioskAppList.prototype = {
    __proto__: List.prototype,

    /**
     * True if auto launch feature can be configured.
     * @type {?boolean}
     */
    autoLaunchEnabled_: false,

    /** @override */
    createItem: function(app) {
      var item = new KioskAppListItem();
      item.data = app;
      item.autoLaunchEnabled = this.autoLaunchEnabled_;
      return item;
    },

    /**
     * Sets auto launch enabled flag.
     * @param {boolean} enabled True if auto launch should be enabled.
     */
    setAutoLaunchEnabled: function(enabled) {
      this.autoLaunchEnabled_ = enabled;
    },

    /**
     * Loads the given list of apps.
     * @param {!Array.<!Object>} apps An array of app info objects.
     */
    setApps: function(apps) {
      this.dataModel = new ArrayDataModel(apps);
    },

    /**
     * Updates the given app.
     * @param {!Object} app An app info object.
     */
    updateApp: function(app) {
      for (var i = 0; i < this.items.length; ++i) {
        if (this.items[i].data.id == app.id) {
          this.items[i].data = app;
          break;
        }
      }
    }
  };

  /**
   * Creates a list item for a kiosk app.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  var KioskAppListItem = cr.ui.define(function() {
    var el = $('kiosk-app-list-item-template').cloneNode(true);
    el.removeAttribute('id');
    el.hidden = false;
    return el;
  });

  KioskAppListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Data object to hold app info.
     * @type {Object}
     * @private
     */
    data_: null,

    get data() {
      assert(this.data_);
      return this.data_;
    },

    set data(data) {
      this.data_ = data;
      this.redraw();
    },

    set autoLaunchEnabled(enabled) {
      this.querySelector('.enable-auto-launch-button').hidden = !enabled;
      this.querySelector('.disable-auto-launch-button').hidden = !enabled;
    },

    /**
     * Getter for the icon element.
     * @type {Element}
     */
    get icon() {
      return this.querySelector('.kiosk-app-icon');
    },

    /**
     * Getter for the name element.
     * @type {Element}
     */
    get name() {
      return this.querySelector('.kiosk-app-name');
    },

    /**
     * Getter for the status text element.
     * @type {Element}
     */
    get status() {
      return this.querySelector('.kiosk-app-status');
    },

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var sendMessageWithId = function(msg) {
        return function() {
          chrome.send(msg, [this.data.id]);
        }.bind(this);
      }.bind(this);

      this.querySelector('.enable-auto-launch-button').onclick =
        sendMessageWithId('enableKioskAutoLaunch');
      this.querySelector('.disable-auto-launch-button').onclick =
        sendMessageWithId('disableKioskAutoLaunch');
      this.querySelector('.row-delete-button').onclick =
          sendMessageWithId('removeKioskApp');
    },

    /**
     * Updates UI from app info data.
     */
    redraw: function() {
      this.icon.classList.toggle('spinner', this.data.isLoading);
      this.icon.style.backgroundImage = 'url(' + this.data.iconURL + ')';

      this.name.textContent = this.data.name || this.data.id;
      this.status.textContent = this.data.autoLaunch ?
          loadTimeData.getString('autoLaunch') : '';

      this.autoLaunch = this.data.autoLaunch;
    }
  };

  /*
   * True if the app represented by this item will auto launch.
   * @type {boolean}
   */
  cr.defineProperty(KioskAppListItem, 'autoLaunch', cr.PropertyKind.BOOL_ATTR);

  // Export
  return {
    KioskAppList: KioskAppList
  };
});
