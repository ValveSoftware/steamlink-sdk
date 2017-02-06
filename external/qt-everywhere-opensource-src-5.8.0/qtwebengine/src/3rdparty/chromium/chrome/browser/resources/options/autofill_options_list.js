// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   guid: string,
 *   label: string,
 *   sublabel: string,
 *   isLocal: boolean,
 *   isCached: boolean
 * }}
 * @see chrome/browser/ui/webui/options/autofill_options_handler.cc
 */
var AutofillEntityMetadata;

cr.define('options.autofillOptions', function() {
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;

  /**
   * @return {!HTMLButtonElement}
   */
  function AutofillEditProfileButton(edit) {
    var editButtonEl = /** @type {HTMLButtonElement} */(
        document.createElement('button'));
    editButtonEl.className =
        'list-inline-button hide-until-hover custom-appearance';
    editButtonEl.textContent =
        loadTimeData.getString('autofillEditProfileButton');
    editButtonEl.onclick = edit;

    editButtonEl.onmousedown = function(e) {
      // Don't select the row when clicking the button.
      e.stopPropagation();
      // Don't focus on the button when clicking it.
      e.preventDefault();
    };

    return editButtonEl;
  }

  /** @return {!Element} */
  function CreateGoogleAccountLabel() {
    var label = document.createElement('div');
    label.className = 'deemphasized hides-on-hover';
    label.textContent = loadTimeData.getString('autofillFromGoogleAccount');
    return label;
  }

  /**
   * Creates a new address list item.
   * @constructor
   * @param {AutofillEntityMetadata} metadata Details about an address profile.
   * @extends {options.DeletableItem}
   * @see chrome/browser/ui/webui/options/autofill_options_handler.cc
   */
  function AddressListItem(metadata) {
    var el = cr.doc.createElement('div');
    el.__proto__ = AddressListItem.prototype;
    /** @private */
    el.metadata_ = metadata;
    el.decorate();

    return el;
  }

  AddressListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.metadata_.label;
      this.contentElement.appendChild(label);

      var sublabel = this.ownerDocument.createElement('div');
      sublabel.className = 'deemphasized';
      sublabel.textContent = this.metadata_.sublabel;
      this.contentElement.appendChild(sublabel);

      if (!this.metadata_.isLocal) {
        this.deletable = false;
        this.contentElement.appendChild(CreateGoogleAccountLabel());
      }

      // The 'Edit' button.
      var metadata = this.metadata_;
      var editButtonEl = AutofillEditProfileButton(
          AddressListItem.prototype.loadAddressEditor.bind(this));
      this.contentElement.appendChild(editButtonEl);
    },

    /**
     * For local Autofill data, this function causes the AutofillOptionsHandler
     * to call showEditAddressOverlay(). For Payments data, the user is
     * redirected to the Payments web interface.
     */
    loadAddressEditor: function() {
      if (this.metadata_.isLocal)
        chrome.send('loadAddressEditor', [this.metadata_.guid]);
      else
        window.open(loadTimeData.getString('paymentsManageAddressesUrl'));
    },
  };

  /**
   * Creates a new credit card list item.
   * @param {AutofillEntityMetadata} metadata Details about a credit card.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function CreditCardListItem(metadata) {
    var el = cr.doc.createElement('div');
    el.__proto__ = CreditCardListItem.prototype;
    /** @private */
    el.metadata_ = metadata;
    el.decorate();

    return el;
  }

  CreditCardListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.metadata_.label;
      this.contentElement.appendChild(label);

      var sublabel = this.ownerDocument.createElement('div');
      sublabel.className = 'deemphasized';
      sublabel.textContent = this.metadata_.sublabel;
      this.contentElement.appendChild(sublabel);

      if (!this.metadata_.isLocal) {
        this.deletable = false;
        this.contentElement.appendChild(CreateGoogleAccountLabel());
      }

      var guid = this.metadata_.guid;
      if (this.metadata_.isCached) {
        var localCopyText = this.ownerDocument.createElement('span');
        localCopyText.className = 'hide-until-hover deemphasized';
        localCopyText.textContent =
            loadTimeData.getString('autofillDescribeLocalCopy');
        this.contentElement.appendChild(localCopyText);

        var clearLocalCopyButton = AutofillEditProfileButton(
            function() { chrome.send('clearLocalCardCopy', [guid]); });
        clearLocalCopyButton.textContent =
            loadTimeData.getString('autofillClearLocalCopyButton');
        this.contentElement.appendChild(clearLocalCopyButton);
      }

      // The 'Edit' button.
      var metadata = this.metadata_;
      var editButtonEl = AutofillEditProfileButton(
          CreditCardListItem.prototype.loadCreditCardEditor.bind(this));
      this.contentElement.appendChild(editButtonEl);
    },

    /**
     * For local Autofill data, this function causes the AutofillOptionsHandler
     * to call showEditCreditCardOverlay(). For Payments data, the user is
     * redirected to the Payments web interface.
     */
    loadCreditCardEditor: function() {
      if (this.metadata_.isLocal)
        chrome.send('loadCreditCardEditor', [this.metadata_.guid]);
      else
        window.open(loadTimeData.getString('paymentsManageInstrumentsUrl'));
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
   * @extends {options.autofillOptions.AutofillProfileList}
   */
  var AutofillAddressList = cr.ui.define('list');

  AutofillAddressList.prototype = {
    __proto__: AutofillProfileList.prototype,

    decorate: function() {
      AutofillProfileList.prototype.decorate.call(this);
    },

    /** @override */
    activateItemAtIndex: function(index) {
      this.getListItemByIndex(index).loadAddressEditor();
    },

    /**
     * @override
     * @param {AutofillEntityMetadata} metadata
     */
    createItem: function(metadata) {
      return new AddressListItem(metadata);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeData(this.dataModel.item(index).guid,
                                 'Options_AutofillAddressDeleted');
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
      this.getListItemByIndex(index).loadCreditCardEditor();
    },

    /**
     * @override
     * @param {AutofillEntityMetadata} metadata
     */
    createItem: function(metadata) {
      return new CreditCardListItem(metadata);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeData(this.dataModel.item(index).guid,
                                 'Options_AutofillCreditCardDeleted');
    },
  };

  return {
    AutofillProfileList: AutofillProfileList,
    AddressListItem: AddressListItem,
    CreditCardListItem: CreditCardListItem,
    AutofillAddressList: AutofillAddressList,
    AutofillCreditCardList: AutofillCreditCardList,
  };
});
