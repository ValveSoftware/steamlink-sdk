// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.supervisedUserOptions', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Create a new supervised user list item.
   * @param {Object} entry The supervised user this item represents.
   *     It has the following form:
   *       supervisedUser = {
   *         id: "Supervised User ID",
   *         name: "Supervised User Name",
   *         iconURL: "chrome://path/to/icon/image",
   *         onCurrentDevice: true or false,
   *         needAvatar: true or false
   *       }
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function SupervisedUserListItem(entry) {
    var el = cr.doc.createElement('div');
    el.className = 'list-item';
    el.supervisedUser_ = entry;
    el.__proto__ = SupervisedUserListItem.prototype;
    el.decorate();
    return el;
  }

  SupervisedUserListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * @type {string} the ID of this supervised user list item.
     */
    get id() {
      return this.supervisedUser_.id;
    },

    /**
     * @type {string} the name of this supervised user list item.
     */
    get name() {
      return this.supervisedUser_.name;
    },

    /**
     * @type {string} the path to the avatar icon of this supervised
     *     user list item.
     */
    get iconURL() {
      return this.supervisedUser_.iconURL;
    },

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      var supervisedUser = this.supervisedUser_;

      // Add the avatar.
      var iconElement = this.ownerDocument.createElement('img');
      iconElement.className = 'profile-img';
      iconElement.style.content = cr.icon.getImage(supervisedUser.iconURL);
      this.appendChild(iconElement);

      // Add the profile name.
      var nameElement = this.ownerDocument.createElement('div');
      nameElement.className = 'profile-name';
      nameElement.textContent = supervisedUser.name;
      this.appendChild(nameElement);

      if (supervisedUser.onCurrentDevice) {
        iconElement.className += ' profile-img-disabled';
        nameElement.className += ' profile-name-disabled';

        // Add "(already on this device)" message.
        var alreadyOnDeviceElement = this.ownerDocument.createElement('div');
        alreadyOnDeviceElement.className =
            'profile-name-disabled already-on-this-device';
        alreadyOnDeviceElement.textContent =
            loadTimeData.getString('supervisedUserAlreadyOnThisDevice');
        this.appendChild(alreadyOnDeviceElement);
      }
    },
  };

  /**
   * Create a new supervised users list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var SupervisedUserList = cr.ui.define('list');

  SupervisedUserList.prototype = {
    __proto__: List.prototype,

    /**
     * @override
     * @param {Object} entry
     */
    createItem: function(entry) {
      return new SupervisedUserListItem(entry);
    },

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.selectionModel = new ListSingleSelectionModel();
      this.autoExpands = true;
    },
  };

  return {
    SupervisedUserListItem: SupervisedUserListItem,
    SupervisedUserList: SupervisedUserList,
  };
});
