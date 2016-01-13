// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.autofillOptions', function() {
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;

  function AutofillEditProfileButton(guid, edit) {
    var editButtonEl = document.createElement('button');
    editButtonEl.className = 'list-inline-button custom-appearance';
    editButtonEl.textContent =
        loadTimeData.getString('autofillEditProfileButton');
    editButtonEl.onclick = function(e) { edit(guid); };

    editButtonEl.onmousedown = function(e) {
      // Don't select the row when clicking the button.
      e.stopPropagation();
      // Don't focus on the button when clicking it.
      e.preventDefault();
    };

    return editButtonEl;
  }

  /**
   * Creates a new address list item.
   * @param {Array} entry An array of the form [guid, label].
   * @constructor
   * @extends {options.DeletableItem}
   */
  function AddressListItem(entry) {
    var el = cr.doc.createElement('div');
    el.guid = entry[0];
    el.label = entry[1];
    el.__proto__ = AddressListItem.prototype;
    el.decorate();

    return el;
  }

  AddressListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The stored label.
      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.label;
      this.contentElement.appendChild(label);

      // The 'Edit' button.
      var editButtonEl = new AutofillEditProfileButton(
        this.guid,
        AutofillOptions.loadAddressEditor);
      this.contentElement.appendChild(editButtonEl);
    },
  };

  /**
   * Creates a new credit card list item.
   * @param {Array} entry An array of the form [guid, label, icon].
   * @constructor
   * @extends {options.DeletableItem}
   */
  function CreditCardListItem(entry) {
    var el = cr.doc.createElement('div');
    el.guid = entry[0];
    el.label = entry[1];
    el.icon = entry[2];
    el.description = entry[3];
    el.__proto__ = CreditCardListItem.prototype;
    el.decorate();

    return el;
  }

  CreditCardListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The stored label.
      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.label;
      this.contentElement.appendChild(label);

      // The credit card icon.
      var icon = this.ownerDocument.createElement('img');
      icon.src = this.icon;
      icon.alt = this.description;
      this.contentElement.appendChild(icon);

      // The 'Edit' button.
      var editButtonEl = new AutofillEditProfileButton(
        this.guid,
        AutofillOptions.loadCreditCardEditor);
      this.contentElement.appendChild(editButtonEl);
    },
  };

  /**
   * Creates a new value list item.
   * @param {AutofillValuesList} list The parent list of this item.
   * @param {string} entry A string value.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function ValuesListItem(list, entry) {
    var el = cr.doc.createElement('div');
    el.list = list;
    el.value = entry ? entry : '';
    el.__proto__ = ValuesListItem.prototype;
    el.decorate();

    return el;
  }

  ValuesListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      // Note: This must be set prior to calling |createEditableTextCell|.
      this.isPlaceholder = !this.value;

      // The stored value.
      var cell = this.createEditableTextCell(this.value);
      this.contentElement.appendChild(cell);
      this.input = cell.querySelector('input');

      if (this.isPlaceholder) {
        this.input.placeholder = this.list.getAttribute('placeholder');
        this.deletable = false;
      }

      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /**
     * @return {string} This item's value.
     * @protected
     */
    value_: function() {
      return this.input.value;
    },

    /**
     * @param {Object} value The value to test.
     * @return {boolean} True if the given value is non-empty.
     * @protected
     */
    valueIsNonEmpty_: function(value) {
      return !!value;
    },

    /**
     * @return {boolean} True if value1 is logically equal to value2.
     */
    valuesAreEqual_: function(value1, value2) {
      return value1 === value2;
    },

    /**
     * Clears the item's value.
     * @protected
     */
    clearValue_: function() {
      this.input.value = '';
    },

    /**
     * Called when committing an edit.
     * If this is an "Add ..." item, committing a non-empty value adds that
     * value to the end of the values list, but also leaves this "Add ..." item
     * in place.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var value = this.value_();
      var i = this.list.items.indexOf(this);
      if (i < this.list.dataModel.length &&
          this.valuesAreEqual_(value, this.list.dataModel.item(i))) {
        return;
      }

      var entries = this.list.dataModel.slice();
      if (this.valueIsNonEmpty_(value) &&
          !entries.some(this.valuesAreEqual_.bind(this, value))) {
        // Update with new value.
        if (this.isPlaceholder) {
          // It is important that updateIndex is done before validateAndSave.
          // Otherwise we can not be sure about AddRow index.
          this.list.dataModel.updateIndex(i);
          this.list.validateAndSave(i, 0, value);
        } else {
          this.list.validateAndSave(i, 1, value);
        }
      } else {
        // Reject empty values and duplicates.
        if (!this.isPlaceholder)
          this.list.dataModel.splice(i, 1);
        else
          this.clearValue_();
      }
    },
  };

  /**
   * Creates a new name value list item.
   * @param {AutofillNameValuesList} list The parent list of this item.
   * @param {array} entry An array of [first, middle, last] names.
   * @constructor
   * @extends {options.ValuesListItem}
   */
  function NameListItem(list, entry) {
    var el = cr.doc.createElement('div');
    el.list = list;
    el.first = entry ? entry[0] : '';
    el.middle = entry ? entry[1] : '';
    el.last = entry ? entry[2] : '';
    el.__proto__ = NameListItem.prototype;
    el.decorate();

    return el;
  }

  NameListItem.prototype = {
    __proto__: ValuesListItem.prototype,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      // Note: This must be set prior to calling |createEditableTextCell|.
      this.isPlaceholder = !this.first && !this.middle && !this.last;

      // The stored value.
      // For the simulated static "input element" to display correctly, the
      // value must not be empty.  We use a space to force the UI to render
      // correctly when the value is logically empty.
      var cell = this.createEditableTextCell(this.first);
      this.contentElement.appendChild(cell);
      this.firstNameInput = cell.querySelector('input');

      cell = this.createEditableTextCell(this.middle);
      this.contentElement.appendChild(cell);
      this.middleNameInput = cell.querySelector('input');

      cell = this.createEditableTextCell(this.last);
      this.contentElement.appendChild(cell);
      this.lastNameInput = cell.querySelector('input');

      if (this.isPlaceholder) {
        this.firstNameInput.placeholder =
            loadTimeData.getString('autofillAddFirstNamePlaceholder');
        this.middleNameInput.placeholder =
            loadTimeData.getString('autofillAddMiddleNamePlaceholder');
        this.lastNameInput.placeholder =
            loadTimeData.getString('autofillAddLastNamePlaceholder');
        this.deletable = false;
      }

      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /** @override */
    value_: function() {
      return [this.firstNameInput.value,
              this.middleNameInput.value,
              this.lastNameInput.value];
    },

    /** @override */
    valueIsNonEmpty_: function(value) {
      return value[0] || value[1] || value[2];
    },

    /** @override */
    valuesAreEqual_: function(value1, value2) {
      // First, check for null values.
      if (!value1 || !value2)
        return value1 == value2;

      return value1[0] === value2[0] &&
             value1[1] === value2[1] &&
             value1[2] === value2[2];
    },

    /** @override */
    clearValue_: function() {
      this.firstNameInput.value = '';
      this.middleNameInput.value = '';
      this.lastNameInput.value = '';
    },
  };

  /**
   * Base class for shared implementation between address and credit card lists.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutofillProfileList = cr.ui.define('list');

  AutofillProfileList.prototype = {
    __proto__: DeletableItemList.prototype,

    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);

      this.addEventListener('blur', this.onBlur_);
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },
  };

  /**
   * Create a new address list.
   * @constructor
   * @extends {options.AutofillProfileList}
   */
  var AutofillAddressList = cr.ui.define('list');

  AutofillAddressList.prototype = {
    __proto__: AutofillProfileList.prototype,

    decorate: function() {
      AutofillProfileList.prototype.decorate.call(this);
    },

    /** @override */
    activateItemAtIndex: function(index) {
      AutofillOptions.loadAddressEditor(this.dataModel.item(index)[0]);
    },

    /** @override */
    createItem: function(entry) {
      return new AddressListItem(entry);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeData(this.dataModel.item(index)[0]);
    },
  };

  /**
   * Create a new credit card list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutofillCreditCardList = cr.ui.define('list');

  AutofillCreditCardList.prototype = {
    __proto__: AutofillProfileList.prototype,

    decorate: function() {
      AutofillProfileList.prototype.decorate.call(this);
    },

    /** @override */
    activateItemAtIndex: function(index) {
      AutofillOptions.loadCreditCardEditor(this.dataModel.item(index)[0]);
    },

    /** @override */
    createItem: function(entry) {
      return new CreditCardListItem(entry);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeData(this.dataModel.item(index)[0]);
    },
  };

  /**
   * Create a new value list.
   * @constructor
   * @extends {options.InlineEditableItemList}
   */
  var AutofillValuesList = cr.ui.define('list');

  AutofillValuesList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /** @override */
    createItem: function(entry) {
      return new ValuesListItem(this, entry);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      this.dataModel.splice(index, 1);
    },

    /** @override */
    shouldFocusPlaceholder: function() {
      return false;
    },

    /**
     * Called when the list hierarchy as a whole loses or gains focus.
     * If the list was focused in response to a mouse click, call into the
     * superclass's implementation.  If the list was focused in response to a
     * keyboard navigation, focus the first item.
     * If the list loses focus, unselect all the elements.
     * @param {Event} e The change event.
     * @private
     */
    handleListFocusChange_: function(e) {
      // We check to see whether there is a selected item as a proxy for
      // distinguishing between mouse- and keyboard-originated focus events.
      var selectedItem = this.selectedItem;
      if (selectedItem)
        InlineEditableItemList.prototype.handleListFocusChange_.call(this, e);

      if (!e.newValue) {
        // When the list loses focus, unselect all the elements.
        this.selectionModel.unselectAll();
      } else {
        // When the list gains focus, select the first item if nothing else is
        // selected.
        var firstItem = this.getListItemByIndex(0);
        if (!selectedItem && firstItem && e.newValue)
          firstItem.handleFocus_();
      }
    },

    /**
     * Called when a new list item should be validated; subclasses are
     * responsible for implementing if validation is required.
     * @param {number} index The index of the item that was inserted or changed.
     * @param {number} remove The number items to remove.
     * @param {string} value The value of the item to insert.
     */
    validateAndSave: function(index, remove, value) {
      this.dataModel.splice(index, remove, value);
    },
  };

  /**
   * Create a new value list for phone number validation.
   * @constructor
   * @extends {options.AutofillValuesList}
   */
  var AutofillNameValuesList = cr.ui.define('list');

  AutofillNameValuesList.prototype = {
    __proto__: AutofillValuesList.prototype,

    /** @override */
    createItem: function(entry) {
      return new NameListItem(this, entry);
    },
  };

  /**
   * Create a new value list for phone number validation.
   * @constructor
   * @extends {options.AutofillValuesList}
   */
  var AutofillPhoneValuesList = cr.ui.define('list');

  AutofillPhoneValuesList.prototype = {
    __proto__: AutofillValuesList.prototype,

    /** @override */
    validateAndSave: function(index, remove, value) {
      var numbers = this.dataModel.slice(0, this.dataModel.length - 1);
      numbers.splice(index, remove, value);
      var info = new Array();
      info[0] = index;
      info[1] = numbers;
      info[2] = document.querySelector(
          '#autofill-edit-address-overlay [field=country]').value;
      this.validationRequests_++;
      chrome.send('validatePhoneNumbers', info);
    },

    /**
     * The number of ongoing validation requests.
     * @type {number}
     * @private
     */
    validationRequests_: 0,

    /**
     * Pending Promise resolver functions.
     * @type {Array.<!Function>}
     * @private
     */
    validationPromiseResolvers_: [],

    /**
     * This should be called when a reply of chrome.send('validatePhoneNumbers')
     * is received.
     */
    didReceiveValidationResult: function() {
      this.validationRequests_--;
      assert(this.validationRequests_ >= 0);
      if (this.validationRequests_ <= 0) {
        while (this.validationPromiseResolvers_.length) {
          this.validationPromiseResolvers_.pop()();
        }
      }
    },

    /**
     * Returns a Promise which is fulfilled when all of validation requests are
     * completed.
     * @return {!Promise} A promise.
     */
    doneValidating: function() {
      if (this.validationRequests_ <= 0)
        return Promise.resolve();
      return new Promise(function(resolve) {
        this.validationPromiseResolvers_.push(resolve);
      }.bind(this));
    }
  };

  return {
    AddressListItem: AddressListItem,
    CreditCardListItem: CreditCardListItem,
    ValuesListItem: ValuesListItem,
    NameListItem: NameListItem,
    AutofillAddressList: AutofillAddressList,
    AutofillCreditCardList: AutofillCreditCardList,
    AutofillValuesList: AutofillValuesList,
    AutofillNameValuesList: AutofillNameValuesList,
    AutofillPhoneValuesList: AutofillPhoneValuesList,
  };
});
