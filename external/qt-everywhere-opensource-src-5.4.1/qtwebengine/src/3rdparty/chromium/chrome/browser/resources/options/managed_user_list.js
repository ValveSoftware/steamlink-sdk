// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.managedUserOptions', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Create a new managed user list item.
   * @param {Object} entry The managed user this item represents.
   *     It has the following form:
   *       managedUser = {
   *         id: "Managed User ID",
   *         name: "Managed User Name",
   *         iconURL: "chrome://path/to/icon/image",
   *         onCurrentDevice: true or false,
   *         needAvatar: true or false
   *       }
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function ManagedUserListItem(entry) {
    var el = cr.doc.createElement('div');
    el.managedUser_ = entry;
    el.__proto__ = ManagedUserListItem.prototype;
    el.decorate();
    return el;
  }

  ManagedUserListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * @type {string} the ID of this managed user list item.
     */
    get id() {
      return this.managedUser_.id;
    },

    /**
     * @type {string} the name of this managed user list item.
     */
    get name() {
      return this.managedUser_.name;
    },

    /**
     * @type {string} the path to the avatar icon of this managed
     *     user list item.
     */
    get iconURL() {
      return this.managedUser_.iconURL;
    },

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      var managedUser = this.managedUser_;

      // Add the avatar.
      var iconElement = this.ownerDocument.createElement('img');
      iconElement.className = 'profile-img';
      iconElement.style.content = getProfileAvatarIcon(managedUser.iconURL);
      this.appendChild(iconElement);

      // Add the profile name.
      var nameElement = this.ownerDocument.createElement('div');
      nameElement.className = 'profile-name';
      nameElement.textContent = managedUser.name;
      this.appendChild(nameElement);

      if (managedUser.onCurrentDevice) {
        iconElement.className += ' profile-img-disabled';
        nameElement.className += ' profile-name-disabled';

        // Add "(already on this device)" message.
        var alreadyOnDeviceElement = this.ownerDocument.createElement('div');
        alreadyOnDeviceElement.className =
            'profile-name-disabled already-on-this-device';
        alreadyOnDeviceElement.textContent =
            loadTimeData.getString('managedUserAlreadyOnThisDevice');
        this.appendChild(alreadyOnDeviceElement);
      }
    },
  };

  /**
   * Create a new managed users list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var ManagedUserList = cr.ui.define('list');

  ManagedUserList.prototype = {
    __proto__: List.prototype,

    /** @override */
    createItem: function(entry) {
      return new ManagedUserListItem(entry);
    },

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.selectionModel = new ListSingleSelectionModel();
      this.autoExpands = true;
    },
  };

  return {
    ManagedUserListItem: ManagedUserListItem,
    ManagedUserList: ManagedUserList,
  };
});
