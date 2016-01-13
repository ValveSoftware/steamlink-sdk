// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.accounts', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new user list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var UserList = cr.ui.define('list');

  UserList.prototype = {
    __proto__: List.prototype,

    pref: 'cros.accounts.users',

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', this.redraw.bind(this));

      var self = this;

      // Listens to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.load_(event.value.value);
          });
    },

    createItem: function(user) {
      return new UserListItem(user);
    },

    /**
     * Finds the index of user by given username (canonicalized email).
     * @private
     * @param {string} username The username to look for.
     * @return {number} The index of the found user or -1 if not found.
     */
    indexOf_: function(username) {
      var dataModel = this.dataModel;
      if (!dataModel)
        return -1;

      var length = dataModel.length;
      for (var i = 0; i < length; ++i) {
        var user = dataModel.item(i);
        if (user.username == username) {
          return i;
        }
      }

      return -1;
    },

    /**
     * Update given user's account picture.
     * @param {string} username User for which to update the image.
     */
    updateAccountPicture: function(username) {
      var index = this.indexOf_(username);
      if (index >= 0) {
        var item = this.getListItemByIndex(index);
        if (item)
          item.updatePicture();
      }
    },

    /**
     * Loads given user list.
     * @param {Array.<Object>} users An array of user info objects.
     * @private
     */
    load_: function(users) {
      this.dataModel = new ArrayDataModel(users);
    },

    /**
     * Removes given user from the list.
     * @param {Object} user User info object to be removed from user list.
     * @private
     */
    removeUser_: function(user) {
      var e = new Event('remove');
      e.user = user;
      this.dispatchEvent(e);
    }
  };

  /**
   * Whether the user list is disabled. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(UserList, 'disabled', cr.PropertyKind.BOOL_ATTR);

  /**
   * Creates a new user list item.
   * @param {Object} user The user account this represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function UserListItem(user) {
    var el = cr.doc.createElement('div');
    el.user = user;
    UserListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a user account item.
   * @param {!HTMLElement} el The element to decorate.
   */
  UserListItem.decorate = function(el) {
    el.__proto__ = UserListItem.prototype;
    el.decorate();
  };

  UserListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      this.className = 'user-list-item';

      this.icon_ = this.ownerDocument.createElement('img');
      this.icon_.className = 'user-icon';
      this.updatePicture();

      var labelEmail = this.ownerDocument.createElement('span');
      labelEmail.className = 'user-email-label';
      labelEmail.textContent = this.user.email;

      var labelName = this.ownerDocument.createElement('span');
      labelName.className = 'user-name-label';
      labelName.textContent = this.user.owner ?
          loadTimeData.getStringF('username_format', this.user.name) :
          this.user.name;

      var emailNameBlock = this.ownerDocument.createElement('div');
      emailNameBlock.className = 'user-email-name-block';
      emailNameBlock.appendChild(labelEmail);
      emailNameBlock.appendChild(labelName);
      emailNameBlock.title = this.user.owner ?
          loadTimeData.getStringF('username_format', this.user.email) :
          this.user.email;

      this.appendChild(this.icon_);
      this.appendChild(emailNameBlock);

      if (!this.user.owner) {
        var removeButton = this.ownerDocument.createElement('button');
        removeButton.className =
            'raw-button remove-user-button custom-appearance';
        removeButton.addEventListener(
            'click', this.handleRemoveButtonClick_.bind(this));
        this.appendChild(removeButton);
      }
    },

    /**
     * Handles click on the remove button.
     * @param {Event} e Click event.
     * @private
     */
    handleRemoveButtonClick_: function(e) {
      // Handle left button click
      if (e.button == 0)
        this.parentNode.removeUser_(this.user);
    },

    /**
     * Reloads user picture.
     */
    updatePicture: function() {
      this.icon_.src = 'chrome://userimage/' + this.user.username +
                       '?id=' + (new Date()).getTime();
    }
  };

  return {
    UserList: UserList
  };
});
