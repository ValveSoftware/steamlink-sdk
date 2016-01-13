// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.network', function() {

  var ArrayDataModel = cr.ui.ArrayDataModel;
  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;
  var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;
  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;
  var ControlledSettingIndicator = options.ControlledSettingIndicator;

  /**
   * Network settings constants. These enums usually match their C++
   * counterparts.
   */
  function Constants() {}

  // Network types:
  Constants.TYPE_UNKNOWN = 'UNKNOWN';
  Constants.TYPE_ETHERNET = 'ethernet';
  Constants.TYPE_WIFI = 'wifi';
  Constants.TYPE_WIMAX = 'wimax';
  Constants.TYPE_BLUETOOTH = 'bluetooth';
  Constants.TYPE_CELLULAR = 'cellular';
  Constants.TYPE_VPN = 'vpn';

  // Cellular activation states:
  Constants.ACTIVATION_STATE_UNKNOWN = 0;
  Constants.ACTIVATION_STATE_ACTIVATED = 1;
  Constants.ACTIVATION_STATE_ACTIVATING = 2;
  Constants.ACTIVATION_STATE_NOT_ACTIVATED = 3;
  Constants.ACTIVATION_STATE_PARTIALLY_ACTIVATED = 4;

  /**
   * Order in which controls are to appear in the network list sorted by key.
   */
  Constants.NETWORK_ORDER = ['ethernet',
                             'wifi',
                             'wimax',
                             'cellular',
                             'vpn',
                             'addConnection'];

  /**
   * Mapping of network category titles to the network type.
   */
  var categoryMap = {
    'cellular': Constants.TYPE_CELLULAR,
    'ethernet': Constants.TYPE_ETHERNET,
    'wimax': Constants.TYPE_WIMAX,
    'wifi': Constants.TYPE_WIFI,
    'vpn': Constants.TYPE_VPN
  };

  /**
   * ID of the menu that is currently visible.
   * @type {?string}
   * @private
   */
  var activeMenu_ = null;

  /**
   * Indicates if cellular networks are available.
   * @type {boolean}
   * @private
   */
  var cellularAvailable_ = false;

  /**
   * Indicates if cellular networks are enabled.
   * @type {boolean}
   * @private
   */
  var cellularEnabled_ = false;

  /**
   * Indicates if cellular device supports network scanning.
   * @type {boolean}
   * @private
   */
  var cellularSupportsScan_ = false;

  /**
   * Indicates if WiMAX networks are available.
   * @type {boolean}
   * @private
   */
  var wimaxAvailable_ = false;

  /**
   * Indicates if WiMAX networks are enabled.
   * @type {boolean}
   * @private
   */
  var wimaxEnabled_ = false;

  /**
   * Indicates if mobile data roaming is enabled.
   * @type {boolean}
   * @private
   */
  var enableDataRoaming_ = false;

  /**
   * Icon to use when not connected to a particular type of network.
   * @type {!Object.<string, string>} Mapping of network type to icon data url.
   * @private
   */
  var defaultIcons_ = {};

  /**
   * Contains the current logged in user type, which is one of 'none',
   * 'regular', 'owner', 'guest', 'retail-mode', 'public-account',
   * 'locally-managed', and 'kiosk-app', or empty string if the data has not
   * been set.
   * @type {string}
   * @private
   */
  var loggedInUserType_ = '';

  /**
   * Create an element in the network list for controlling network
   * connectivity.
   * @param {Object} data Description of the network list or command.
   * @constructor
   */
  function NetworkListItem(data) {
    var el = cr.doc.createElement('li');
    el.data_ = {};
    for (var key in data)
      el.data_[key] = data[key];
    NetworkListItem.decorate(el);
    return el;
  }

  /**
   * Decorate an element as a NetworkListItem.
   * @param {!Element} el The element to decorate.
   */
  NetworkListItem.decorate = function(el) {
    el.__proto__ = NetworkListItem.prototype;
    el.decorate();
  };

  NetworkListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Description of the network group or control.
     * @type {Object.<string,Object>}
     * @private
     */
    data_: null,

    /**
     * Element for the control's subtitle.
     * @type {?Element}
     * @private
     */
    subtitle_: null,

    /**
     * Icon for the network control.
     * @type {?Element}
     * @private
     */
    icon_: null,

    /**
     * Indicates if in the process of connecting to a network.
     * @type {boolean}
     * @private
     */
    connecting_: false,

    /**
     * Description of the network control.
     * @type {Object}
     */
    get data() {
      return this.data_;
    },

    /**
     * Text label for the subtitle.
     * @type {string}
     */
    set subtitle(text) {
      if (text)
        this.subtitle_.textContent = text;
      this.subtitle_.hidden = !text;
    },

    /**
     * URL for the network icon.
     * @type {string}
     */
    set iconURL(iconURL) {
      this.icon_.style.backgroundImage = url(iconURL);
    },

    /**
     * Type of network icon.  Each type corresponds to a CSS rule.
     * @type {string}
     */
    set iconType(type) {
      if (defaultIcons_[type])
        this.iconURL = defaultIcons_[type];
      else
        this.icon_.classList.add('network-' + type);
    },

    /**
     * Indicates if the network is in the process of being connected.
     * @type {boolean}
     */
    set connecting(state) {
      this.connecting_ = state;
      if (state)
        this.icon_.classList.add('network-connecting');
      else
        this.icon_.classList.remove('network-connecting');
    },

    /**
     * Indicates if the network is in the process of being connected.
     * @type {boolean}
     */
    get connecting() {
      return this.connecting_;
    },

    /**
     * Set the direction of the text.
     * @param {string} direction The direction of the text, e.g. 'ltr'.
     */
    setSubtitleDirection: function(direction) {
      this.subtitle_.dir = direction;
    },

    /**
     * Indicate that the selector arrow should be shown.
     */
    showSelector: function() {
      this.subtitle_.classList.add('network-selector');
    },

    /**
     * Adds an indicator to show that the network is policy managed.
     */
    showManagedNetworkIndicator: function() {
      this.appendChild(new ManagedNetworkIndicator());
    },

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      this.className = 'network-group';
      this.icon_ = this.ownerDocument.createElement('div');
      this.icon_.className = 'network-icon';
      this.appendChild(this.icon_);
      var textContent = this.ownerDocument.createElement('div');
      textContent.className = 'network-group-labels';
      this.appendChild(textContent);
      var categoryLabel = this.ownerDocument.createElement('div');
      var title = this.data_.key + 'Title';
      categoryLabel.className = 'network-title';
      categoryLabel.textContent = loadTimeData.getString(title);
      textContent.appendChild(categoryLabel);
      this.subtitle_ = this.ownerDocument.createElement('div');
      this.subtitle_.className = 'network-subtitle';
      textContent.appendChild(this.subtitle_);
    },
  };

  /**
   * Creates a control that displays a popup menu when clicked.
   * @param {Object} data  Description of the control.
   */
  function NetworkMenuItem(data) {
    var el = new NetworkListItem(data);
    el.__proto__ = NetworkMenuItem.prototype;
    el.decorate();
    return el;
  }

  NetworkMenuItem.prototype = {
    __proto__: NetworkListItem.prototype,

    /**
     * Popup menu element.
     * @type {?Element}
     * @private
     */
    menu_: null,

    /** @override */
    decorate: function() {
      this.subtitle = null;
      if (this.data.iconType)
        this.iconType = this.data.iconType;
      this.addEventListener('click', function() {
        this.showMenu();
      });
    },

    /**
     * Retrieves the ID for the menu.
     */
    getMenuName: function() {
      return this.data_.key + '-network-menu';
    },

    /**
     * Creates a popup menu for the control.
     * @return {Element} The newly created menu.
     */
    createMenu: function() {
      if (this.data.menu) {
        var menu = this.ownerDocument.createElement('div');
        menu.id = this.getMenuName();
        menu.className = 'network-menu';
        menu.hidden = true;
        Menu.decorate(menu);
        for (var i = 0; i < this.data.menu.length; i++) {
          var entry = this.data.menu[i];
          createCallback_(menu, null, entry.label, entry.command);
        }
        return menu;
      }
      return null;
    },

    canUpdateMenu: function() {
      return false;
    },

    /**
     * Displays a popup menu.
     */
    showMenu: function() {
      var rebuild = false;
      // Force a rescan if opening the menu for WiFi networks to ensure the
      // list is up to date. Networks are periodically rescanned, but depending
      // on timing, there could be an excessive delay before the first rescan
      // unless forced.
      var rescan = !activeMenu_ && this.data_.key == 'wifi';
      if (!this.menu_) {
        rebuild = true;
        var existing = $(this.getMenuName());
        if (existing) {
          if (this.updateMenu())
            return;
          closeMenu_();
        }
        this.menu_ = this.createMenu();
        this.menu_.addEventListener('mousedown', function(e) {
          // Prevent blurring of list, which would close the menu.
          e.preventDefault();
        });
        var parent = $('network-menus');
        if (existing)
          parent.replaceChild(this.menu_, existing);
        else
          parent.appendChild(this.menu_);
      }
      var top = this.offsetTop + this.clientHeight;
      var menuId = this.getMenuName();
      if (menuId != activeMenu_ || rebuild) {
        closeMenu_();
        activeMenu_ = menuId;
        this.menu_.style.setProperty('top', top + 'px');
        this.menu_.hidden = false;
      }
      if (rescan)
        chrome.send('refreshNetworks');
    },
  };

  /**
   * Creates a control for selecting or configuring a network connection based
   * on the type of connection (e.g. wifi versus vpn).
   * @param {{key: string,
   *          networkList: Array.<Object>} data  Description of the network.
   * @constructor
   */
  function NetworkSelectorItem(data) {
    var el = new NetworkMenuItem(data);
    el.__proto__ = NetworkSelectorItem.prototype;
    el.decorate();
    return el;
  }

  NetworkSelectorItem.prototype = {
    __proto__: NetworkMenuItem.prototype,

    /** @override */
    decorate: function() {
      // TODO(kevers): Generalize method of setting default label.
      var policyManaged = false;
      var defaultMessage = this.data_.key == 'wifi' ?
          'networkOffline' : 'networkNotConnected';
      this.subtitle = loadTimeData.getString(defaultMessage);
      var list = this.data_.networkList;
      var candidateURL = null;
      for (var i = 0; i < list.length; i++) {
        var networkDetails = list[i];
        if (networkDetails.connecting || networkDetails.connected) {
          this.subtitle = networkDetails.networkName;
          this.setSubtitleDirection('ltr');
          policyManaged = networkDetails.policyManaged;
          candidateURL = networkDetails.iconURL;
          // Only break when we see a connecting network as it is possible to
          // have a connected network and a connecting network at the same
          // time.
          if (networkDetails.connecting) {
            this.connecting = true;
            candidateURL = null;
            break;
          }
        }
      }
      if (candidateURL)
        this.iconURL = candidateURL;
      else
        this.iconType = this.data.key;

      this.showSelector();

      if (policyManaged)
        this.showManagedNetworkIndicator();

      if (activeMenu_ == this.getMenuName()) {
        // Menu is already showing and needs to be updated. Explicitly calling
        // show menu will force the existing menu to be replaced.  The call
        // is deferred in order to ensure that position of this element has
        // beem properly updated.
        var self = this;
        setTimeout(function() {self.showMenu();}, 0);
      }
    },

    /**
     * Creates a menu for selecting, configuring or disconnecting from a
     * network.
     * @return {Element} The newly created menu.
     */
    createMenu: function() {
      var menu = this.ownerDocument.createElement('div');
      menu.id = this.getMenuName();
      menu.className = 'network-menu';
      menu.hidden = true;
      Menu.decorate(menu);
      var addendum = [];
      if (this.data_.key == 'wifi') {
        addendum.push({label: loadTimeData.getString('joinOtherNetwork'),
                       command: 'add',
                       data: {networkType: Constants.TYPE_WIFI,
                              servicePath: ''}});
      } else if (this.data_.key == 'cellular') {
        if (cellularEnabled_ && cellularSupportsScan_) {
          entry = {
            label: loadTimeData.getString('otherCellularNetworks'),
            command: createAddConnectionCallback_(Constants.TYPE_CELLULAR),
            addClass: ['other-cellulars'],
            data: {}
          };
          addendum.push(entry);
        }

        var label = enableDataRoaming_ ? 'disableDataRoaming' :
            'enableDataRoaming';
        var disabled = loggedInUserType_ != 'owner';
        var entry = {label: loadTimeData.getString(label),
                     data: {}};
        if (disabled) {
          entry.command = null;
          entry.tooltip =
              loadTimeData.getString('dataRoamingDisableToggleTooltip');
        } else {
          var self = this;
          entry.command = function() {
            options.Preferences.setBooleanPref(
                'cros.signed.data_roaming_enabled',
                !enableDataRoaming_, true);
            // Force revalidation of the menu the next time it is displayed.
            self.menu_ = null;
          };
        }
        addendum.push(entry);
      }
      var list = this.data.rememberedNetworks;
      if (list && list.length > 0) {
        var callback = function(list) {
          $('remembered-network-list').clear();
          var dialog = options.PreferredNetworks.getInstance();
          OptionsPage.showPageByName('preferredNetworksPage', false);
          dialog.update(list);
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_NetworkShowPreferred']);
        };
        addendum.push({label: loadTimeData.getString('preferredNetworks'),
                       command: callback,
                       data: list});
      }

      var networkGroup = this.ownerDocument.createElement('div');
      networkGroup.className = 'network-menu-group';
      list = this.data.networkList;
      var empty = !list || list.length == 0;
      if (list) {
        for (var i = 0; i < list.length; i++) {
          var data = list[i];
          this.createNetworkOptionsCallback_(networkGroup, data);
          if (data.connected) {
            if (data.networkType == Constants.TYPE_VPN) {
              // Add separator
              addendum.push({});
              var i18nKey = 'disconnectNetwork';
              addendum.push({label: loadTimeData.getString(i18nKey),
                             command: 'disconnect',
                             data: data});
            }
          }
        }
      }
      if (this.data_.key == 'wifi' || this.data_.key == 'wimax' ||
              this.data_.key == 'cellular') {
        addendum.push({});
        if (this.data_.key == 'wifi') {
          addendum.push({label: loadTimeData.getString('turnOffWifi'),
                       command: function() {
                         chrome.send('disableWifi');
                       },
                       data: {}});
        } else if (this.data_.key == 'wimax') {
          addendum.push({label: loadTimeData.getString('turnOffWimax'),
                       command: function() {
                         chrome.send('disableWimax');
                       },
                       data: {}});
        } else if (this.data_.key == 'cellular') {
          addendum.push({label: loadTimeData.getString('turnOffCellular'),
                       command: function() {
                         chrome.send('disableCellular');
                       },
                       data: {}});
        }
      }
      if (!empty)
        menu.appendChild(networkGroup);
      if (addendum.length > 0) {
        var separator = false;
        if (!empty) {
          menu.appendChild(MenuItem.createSeparator());
          separator = true;
        }
        for (var i = 0; i < addendum.length; i++) {
          var value = addendum[i];
          if (value.data) {
            var item = createCallback_(menu, value.data, value.label,
                                       value.command);
            if (value.tooltip)
              item.title = value.tooltip;
            if (value.addClass)
              item.classList.add(value.addClass);
            separator = false;
          } else if (!separator) {
            menu.appendChild(MenuItem.createSeparator());
            separator = true;
          }
        }
      }
      return menu;
    },

    /**
     * Determines if a menu can be updated on the fly. Menus that cannot be
     * updated are fully regenerated using createMenu. The advantage of
     * updating a menu is that it can preserve ordering of networks avoiding
     * entries from jumping around after an update.
     */
    canUpdateMenu: function() {
      return this.data_.key == 'wifi' && activeMenu_ == this.getMenuName();
    },

    /**
     * Updates an existing menu.  Updated menus preserve ordering of prior
     * entries.  During the update process, the ordering may differ from the
     * preferred ordering as determined by the network library.  If the
     * ordering becomes potentially out of sync, then the updated menu is
     * marked for disposal on close.  Reopening the menu will force a
     * regeneration, which will in turn fix the ordering.
     * @return {boolean} True if successfully updated.
     */
    updateMenu: function() {
      if (!this.canUpdateMenu())
        return false;
      var oldMenu = $(this.getMenuName());
      var group = oldMenu.getElementsByClassName('network-menu-group')[0];
      if (!group)
        return false;
      var newMenu = this.createMenu();
      var discardOnClose = false;
      var oldNetworkButtons = this.extractNetworkConnectButtons_(oldMenu);
      var newNetworkButtons = this.extractNetworkConnectButtons_(newMenu);
      for (var key in oldNetworkButtons) {
        if (newNetworkButtons[key]) {
          group.replaceChild(newNetworkButtons[key].button,
                             oldNetworkButtons[key].button);
          if (newNetworkButtons[key].index != oldNetworkButtons[key].index)
            discardOnClose = true;
          newNetworkButtons[key] = null;
        } else {
          // Leave item in list to prevent network items from jumping due to
          // deletions.
          oldNetworkButtons[key].disabled = true;
          discardOnClose = true;
        }
      }
      for (var key in newNetworkButtons) {
        var entry = newNetworkButtons[key];
        if (entry) {
          group.appendChild(entry.button);
          discardOnClose = true;
        }
      }
      oldMenu.data = {discardOnClose: discardOnClose};
      return true;
    },

    /**
     * Extracts a mapping of network names to menu element and position.
     * @param {!Element} menu The menu to process.
     * @return {Object.<string, Element>} Network mapping.
     * @private
     */
    extractNetworkConnectButtons_: function(menu) {
      var group = menu.getElementsByClassName('network-menu-group')[0];
      var networkButtons = {};
      if (!group)
        return networkButtons;
      var buttons = group.getElementsByClassName('network-menu-item');
      for (var i = 0; i < buttons.length; i++) {
        var label = buttons[i].data.label;
        networkButtons[label] = {index: i, button: buttons[i]};
      }
      return networkButtons;
    },

    /**
     * Adds a menu item for showing network details.
     * @param {!Element} parent The parent element.
     * @param {Object} data Description of the network.
     * @private
     */
    createNetworkOptionsCallback_: function(parent, data) {
      var menuItem = createCallback_(parent,
                                     data,
                                     data.networkName,
                                     'options',
                                     data.iconURL);
      if (data.policyManaged)
        menuItem.appendChild(new ManagedNetworkIndicator());
      if (data.connected || data.connecting) {
        var label = menuItem.getElementsByClassName(
            'network-menu-item-label')[0];
        label.classList.add('active-network');
      }
    }
  };

  /**
   * Creates a button-like control for configurating internet connectivity.
   * @param {{key: string,
   *          subtitle: string,
   *          command: function} data  Description of the network control.
   * @constructor
   */
  function NetworkButtonItem(data) {
    var el = new NetworkListItem(data);
    el.__proto__ = NetworkButtonItem.prototype;
    el.decorate();
    return el;
  }

  NetworkButtonItem.prototype = {
    __proto__: NetworkListItem.prototype,

    /** @override */
    decorate: function() {
      if (this.data.subtitle)
        this.subtitle = this.data.subtitle;
      else
       this.subtitle = null;
      if (this.data.command)
        this.addEventListener('click', this.data.command);
      if (this.data.iconURL)
        this.iconURL = this.data.iconURL;
      else if (this.data.iconType)
        this.iconType = this.data.iconType;
      if (this.data.policyManaged)
        this.showManagedNetworkIndicator();
    },
  };

  /**
   * Adds a command to a menu for modifying network settings.
   * @param {!Element} menu Parent menu.
   * @param {!Object} data Description of the network.
   * @param {!string} label Display name for the menu item.
   * @param {?(string|function)} command Callback function or name
   *     of the command for |networkCommand|.
   * @param {?string=} opt_iconURL Optional URL to an icon for the menu item.
   * @return {!Element} The created menu item.
   * @private
   */
  function createCallback_(menu, data, label, command, opt_iconURL) {
    var button = menu.ownerDocument.createElement('div');
    button.className = 'network-menu-item';

    var buttonIcon = menu.ownerDocument.createElement('div');
    buttonIcon.className = 'network-menu-item-icon';
    button.appendChild(buttonIcon);
    if (opt_iconURL)
      buttonIcon.style.backgroundImage = url(opt_iconURL);

    var buttonLabel = menu.ownerDocument.createElement('span');
    buttonLabel.className = 'network-menu-item-label';
    buttonLabel.textContent = label;
    button.appendChild(buttonLabel);
    var callback = null;
    if (typeof command == 'string') {
      var type = data.networkType;
      var path = data.servicePath;
      callback = function() {
        chrome.send('networkCommand',
                    [type, path, command]);
        closeMenu_();
      };
    } else if (command != null) {
      if (data) {
        callback = function() {
          command(data);
          closeMenu_();
        };
      } else {
        callback = function() {
          command();
          closeMenu_();
        };
      }
    }
    if (callback != null)
      button.addEventListener('click', callback);
    else
      buttonLabel.classList.add('network-disabled-control');

    button.data = {label: label};
    MenuItem.decorate(button);
    menu.appendChild(button);
    return button;
  }

  /**
   * A list of controls for manipulating network connectivity.
   * @constructor
   */
  var NetworkList = cr.ui.define('list');

  NetworkList.prototype = {
    __proto__: List.prototype,

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.startBatchUpdates();
      this.autoExpands = true;
      this.dataModel = new ArrayDataModel([]);
      this.selectionModel = new ListSingleSelectionModel();
      this.addEventListener('blur', this.onBlur_.bind(this));
      this.selectionModel.addEventListener('change',
                                           this.onSelectionChange_.bind(this));

      // Wi-Fi control is always visible.
      this.update({key: 'wifi', networkList: []});

      var entryAddWifi = {
        label: loadTimeData.getString('addConnectionWifi'),
        command: createAddConnectionCallback_(Constants.TYPE_WIFI)
      };
      var entryAddVPN = {
        label: loadTimeData.getString('addConnectionVPN'),
        command: createAddConnectionCallback_(Constants.TYPE_VPN)
      };
      this.update({key: 'addConnection',
                   iconType: 'add-connection',
                   menu: [entryAddWifi, entryAddVPN]
                  });

      var prefs = options.Preferences.getInstance();
      prefs.addEventListener('cros.signed.data_roaming_enabled',
          function(event) {
            enableDataRoaming_ = event.value.value;
          });
      this.endBatchUpdates();
    },

    /**
     * When the list loses focus, unselect all items in the list and close the
     * active menu.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
      closeMenu_();
    },

    /**
     * Close bubble and menu when a different list item is selected.
     * @param {Event} event Event detailing the selection change.
     * @private
     */
    onSelectionChange_: function(event) {
      OptionsPage.hideBubble();
      // A list item may temporarily become unselected while it is constructing
      // its menu. The menu should therefore only be closed if a different item
      // is selected, not when the menu's owner item is deselected.
      if (activeMenu_) {
        for (var i = 0; i < event.changes.length; ++i) {
          if (event.changes[i].selected) {
            var item = this.dataModel.item(event.changes[i].index);
            if (!item.getMenuName || item.getMenuName() != activeMenu_) {
              closeMenu_();
              return;
            }
          }
        }
      }
    },

    /**
     * Finds the index of a network item within the data model based on
     * category.
     * @param {string} key Unique key for the item in the list.
     * @return {number} The index of the network item, or |undefined| if it is
     *     not found.
     */
    indexOf: function(key) {
      var size = this.dataModel.length;
      for (var i = 0; i < size; i++) {
        var entry = this.dataModel.item(i);
        if (entry.key == key)
          return i;
      }
    },

    /**
     * Updates a network control.
     * @param {Object.<string,string>} data Description of the entry.
     */
    update: function(data) {
      this.startBatchUpdates();
      var index = this.indexOf(data.key);
      if (index == undefined) {
        // Find reference position for adding the element.  We cannot hide
        // individual list elements, thus we need to conditionally add or
        // remove elements and cannot rely on any element having a fixed index.
        for (var i = 0; i < Constants.NETWORK_ORDER.length; i++) {
          if (data.key == Constants.NETWORK_ORDER[i]) {
            data.sortIndex = i;
            break;
          }
        }
        var referenceIndex = -1;
        for (var i = 0; i < this.dataModel.length; i++) {
          var entry = this.dataModel.item(i);
          if (entry.sortIndex < data.sortIndex)
            referenceIndex = i;
          else
            break;
        }
        if (referenceIndex == -1) {
          // Prepend to the start of the list.
          this.dataModel.splice(0, 0, data);
        } else if (referenceIndex == this.dataModel.length) {
          // Append to the end of the list.
          this.dataModel.push(data);
        } else {
          // Insert after the reference element.
          this.dataModel.splice(referenceIndex + 1, 0, data);
        }
      } else {
        var entry = this.dataModel.item(index);
        data.sortIndex = entry.sortIndex;
        this.dataModel.splice(index, 1, data);
      }
      this.endBatchUpdates();
    },

    /** @override */
    createItem: function(entry) {
      if (entry.networkList)
        return new NetworkSelectorItem(entry);
      if (entry.command)
        return new NetworkButtonItem(entry);
      if (entry.menu)
        return new NetworkMenuItem(entry);
    },

    /**
     * Deletes an element from the list.
     * @param {string} key  Unique identifier for the element.
     */
    deleteItem: function(key) {
      var index = this.indexOf(key);
      if (index != undefined)
        this.dataModel.splice(index, 1);
    },

    /**
     * Updates the state of a toggle button.
     * @param {string} key Unique identifier for the element.
     * @param {boolean} active Whether the control is active.
     */
    updateToggleControl: function(key, active) {
      var index = this.indexOf(key);
      if (index != undefined) {
        var entry = this.dataModel.item(index);
        entry.iconType = active ? 'control-active' :
            'control-inactive';
        this.update(entry);
      }
    }
  };

  /**
   * Sets the default icon to use for each network type if disconnected.
   * @param {!Object.<string, string>} data Mapping of network type to icon
   *     data url.
   */
  NetworkList.setDefaultNetworkIcons = function(data) {
    defaultIcons_ = Object.create(data);
  };

  /**
   * Sets the current logged in user type.
   * @param {string} userType Current logged in user type.
   */
  NetworkList.updateLoggedInUserType = function(userType) {
    loggedInUserType_ = String(userType);
  };

  /**
   * Chrome callback for updating network controls.
   * @param {Object} data Description of available network devices and their
   *     corresponding state.
   */
  NetworkList.refreshNetworkData = function(data) {
    var networkList = $('network-list');
    networkList.startBatchUpdates();
    cellularAvailable_ = data.cellularAvailable;
    cellularEnabled_ = data.cellularEnabled;
    cellularSupportsScan_ = data.cellularSupportsScan;
    wimaxAvailable_ = data.wimaxAvailable;
    wimaxEnabled_ = data.wimaxEnabled;

    // Only show Ethernet control if connected.
    var ethernetConnection = getConnection_(data.wiredList);
    if (ethernetConnection) {
      var type = String(Constants.TYPE_ETHERNET);
      var path = ethernetConnection.servicePath;
      var ethernetOptions = function() {
        chrome.send('networkCommand',
                    [type, path, 'options']);
      };
      networkList.update({key: 'ethernet',
                          subtitle: loadTimeData.getString('networkConnected'),
                          iconURL: ethernetConnection.iconURL,
                          command: ethernetOptions,
                          policyManaged: ethernetConnection.policyManaged});
    } else {
      networkList.deleteItem('ethernet');
    }

    if (data.wifiEnabled)
      loadData_('wifi', data.wirelessList, data.rememberedList);
    else
      addEnableNetworkButton_('wifi', 'enableWifi', 'wifi');

    // Only show cellular control if available.
    if (data.cellularAvailable) {
      if (data.cellularEnabled)
        loadData_('cellular', data.wirelessList, data.rememberedList);
      else
        addEnableNetworkButton_('cellular', 'enableCellular', 'cellular');
    } else {
      networkList.deleteItem('cellular');
    }

    // Only show cellular control if available.
    if (data.wimaxAvailable) {
      if (data.wimaxEnabled)
        loadData_('wimax', data.wirelessList, data.rememberedList);
      else
        addEnableNetworkButton_('wimax', 'enableWimax', 'cellular');
    } else {
      networkList.deleteItem('wimax');
    }

    // Only show VPN control if there is at least one VPN configured.
    if (data.vpnList.length > 0)
      loadData_('vpn', data.vpnList, data.rememberedList);
    else
      networkList.deleteItem('vpn');
    networkList.endBatchUpdates();
  };

  /**
   * Replaces a network menu with a button for reenabling the type of network.
   * @param {string} name The type of network (wifi, cellular or wimax).
   * @param {string} command The command for reenabling the network.
   * @param {string} type of icon (wifi or cellular).
   * @private
   */
  function addEnableNetworkButton_(name, command, icon) {
    var subtitle = loadTimeData.getString('networkDisabled');
    var enableNetwork = function() {
      chrome.send(command);
    };
    var networkList = $('network-list');
    networkList.update({key: name,
                        subtitle: subtitle,
                        iconType: icon,
                        command: enableNetwork});
  }

  /**
   * Element for indicating a policy managed network.
   * @constructor
   */
  function ManagedNetworkIndicator() {
    var el = cr.doc.createElement('span');
    el.__proto__ = ManagedNetworkIndicator.prototype;
    el.decorate();
    return el;
  }

  ManagedNetworkIndicator.prototype = {
    __proto__: ControlledSettingIndicator.prototype,

    /** @override */
    decorate: function() {
      ControlledSettingIndicator.prototype.decorate.call(this);
      this.controlledBy = 'policy';
      var policyLabel = loadTimeData.getString('managedNetwork');
      this.setAttribute('textPolicy', policyLabel);
      this.removeAttribute('tabindex');
    },

    /** @override */
    handleEvent: function(event) {
      // Prevent focus blurring as that would close any currently open menu.
      if (event.type == 'mousedown')
        return;
      ControlledSettingIndicator.prototype.handleEvent.call(this, event);
    },

    /**
     * Handle mouse events received by the bubble, preventing focus blurring as
     * that would close any currently open menu and preventing propagation to
     * any elements located behind the bubble.
     * @param {Event} Mouse event.
     */
    stopEvent: function(event) {
      event.preventDefault();
      event.stopPropagation();
    },

    /** @override */
    toggleBubble_: function() {
      if (activeMenu_ && !$(activeMenu_).contains(this))
        closeMenu_();
      ControlledSettingIndicator.prototype.toggleBubble_.call(this);
      if (this.showingBubble) {
        var bubble = OptionsPage.getVisibleBubble();
        bubble.addEventListener('mousedown', this.stopEvent);
        bubble.addEventListener('click', this.stopEvent);
      }
    },
  };

  /**
   * Updates the list of available networks and their status, filtered by
   * network type.
   * @param {string} category The type of network.
   * @param {Array} available The list of available networks and their status.
   * @param {Array} remembered The list of remmebered networks.
   */
  function loadData_(category, available, remembered) {
    var data = {key: category};
    var type = categoryMap[category];
    var availableNetworks = [];
    for (var i = 0; i < available.length; i++) {
      if (available[i].networkType == type)
        availableNetworks.push(available[i]);
    }
    data.networkList = availableNetworks;
    if (remembered) {
      var rememberedNetworks = [];
      for (var i = 0; i < remembered.length; i++) {
        if (remembered[i].networkType == type)
          rememberedNetworks.push(remembered[i]);
      }
      data.rememberedNetworks = rememberedNetworks;
    }
    $('network-list').update(data);
  }

  /**
   * Hides the currently visible menu.
   * @private
   */
  function closeMenu_() {
    if (activeMenu_) {
      var menu = $(activeMenu_);
      menu.hidden = true;
      if (menu.data && menu.data.discardOnClose)
        menu.parentNode.removeChild(menu);
      activeMenu_ = null;
    }
  }

  /**
   * Fetches the active connection.
   * @param {Array.<Object>} networkList List of networks.
   * @return {boolean} True if connected or connecting to a network.
   * @private
   */
  function getConnection_(networkList) {
    if (!networkList)
      return null;
    for (var i = 0; i < networkList.length; i++) {
      var entry = networkList[i];
      if (entry.connected || entry.connecting)
        return entry;
    }
    return null;
  }

  /**
   * Create a callback function that adds a new connection of the given type.
   * @param {!number} type A network type Constants.TYPE_*.
   * @return {function()} The created callback.
   * @private
   */
  function createAddConnectionCallback_(type) {
    return function() {
      chrome.send('networkCommand', [String(type), '', 'add']);
    };
  }

  /**
   * Whether the Network list is disabled. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkList, 'disabled', cr.PropertyKind.BOOL_ATTR);

  // Export
  return {
    NetworkList: NetworkList
  };
});
