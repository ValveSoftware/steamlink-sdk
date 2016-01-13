// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * AutofillEditAddressOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutofillEditAddressOverlay() {
    OptionsPage.call(this, 'autofillEditAddress',
                     loadTimeData.getString('autofillEditAddressTitle'),
                     'autofill-edit-address-overlay');
  }

  cr.addSingletonGetter(AutofillEditAddressOverlay);

  AutofillEditAddressOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The GUID of the loaded address.
     * @type {string}
     */
    guid_: '',

    /**
     * The BCP 47 language code for the layout of input fields.
     * @type {string}
     */
    languageCode_: '',

    /**
     * The saved field values for the address. For example, if the user changes
     * from United States to Switzerland, then the State field will be hidden
     * and its value will be stored here. If the user changes back to United
     * States, then the State field will be restored to its previous value, as
     * stored in this object.
     * @type {Object}
     */
    savedFieldValues_: {},

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createMultiValueLists_();

      var self = this;
      $('autofill-edit-address-cancel-button').onclick = function(event) {
        self.dismissOverlay_();
      };

      // TODO(jhawkins): Investigate other possible solutions.
      $('autofill-edit-address-apply-button').onclick = function(event) {
        // Blur active element to ensure that pending changes are committed.
        if (document.activeElement)
          document.activeElement.blur();
        // Blurring is delayed for list elements.  Queue save and close to
        // ensure that pending changes have been applied.
        setTimeout(function() {
          self.pageDiv.querySelector('[field=phone]').doneValidating().then(
              function() {
                self.saveAddress_();
                self.dismissOverlay_();
              });
        }, 0);
      };

      // Prevent 'blur' events on the OK and cancel buttons, which can trigger
      // insertion of new placeholder elements.  The addition of placeholders
      // affects layout, which interferes with being able to click on the
      // buttons.
      $('autofill-edit-address-apply-button').onmousedown = function(event) {
        event.preventDefault();
      };
      $('autofill-edit-address-cancel-button').onmousedown = function(event) {
        event.preventDefault();
      };

      this.guid_ = '';
      this.populateCountryList_();
      this.rebuildInputFields_(
          loadTimeData.getValue('autofillDefaultCountryComponents'));
      this.languageCode_ =
          loadTimeData.getString('autofillDefaultCountryLanguageCode');
      this.connectInputEvents_();
      this.setInputFields_({});
      this.getCountrySwitcher_().onchange = function(event) {
        self.countryChanged_();
      };
    },

    /**
    * Specifically catch the situations in which the overlay is cancelled
    * externally (e.g. by pressing <Esc>), so that the input fields and
    * GUID can be properly cleared.
    * @override
    */
    handleCancel: function() {
      this.dismissOverlay_();
    },

    /**
     * Creates, decorates and initializes the multi-value lists for phone and
     * email.
     * @private
     */
    createMultiValueLists_: function() {
      var list = this.pageDiv.querySelector('[field=phone]');
      options.autofillOptions.AutofillPhoneValuesList.decorate(list);
      list.autoExpands = true;

      list = this.pageDiv.querySelector('[field=email]');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;
    },

    /**
     * Updates the data model for the |list| with the values from |entries|.
     * @param {cr.ui.List} list The list to update.
     * @param {Array} entries The list of items to be added to the list.
     * @private
     */
    setMultiValueList_: function(list, entries) {
      // Add special entry for adding new values.
      var augmentedList = entries.slice();
      augmentedList.push(null);
      list.dataModel = new ArrayDataModel(augmentedList);

      // Update the status of the 'OK' button.
      this.inputFieldChanged_();

      list.dataModel.addEventListener('splice',
                                      this.inputFieldChanged_.bind(this));
      list.dataModel.addEventListener('change',
                                      this.inputFieldChanged_.bind(this));
    },

    /**
     * Clears any uncommitted input, resets the stored GUID and dismisses the
     * overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.setInputFields_({});
      this.inputFieldChanged_();
      this.guid_ = '';
      this.languageCode_ = '';
      this.savedInputFields_ = {};
      OptionsPage.closeOverlay();
    },

    /**
     * @return {Element} The element used to switch countries.
     * @private
     */
    getCountrySwitcher_: function() {
      return this.pageDiv.querySelector('[field=country]');
    },

    /**
     * Returns all list elements.
     * @return {!NodeList} The list elements.
     * @private
     */
    getLists_: function() {
      return this.pageDiv.querySelectorAll('list[field]');
    },

    /**
     * Returns all text input elements.
     * @return {!NodeList} The text input elements.
     * @private
     */
    getTextFields_: function() {
      return this.pageDiv.querySelectorAll('textarea[field], input[field]');
    },

    /**
     * Creates a map from type => value for all text fields.
     * @return {Object} The mapping from field names to values.
     * @private
     */
    getInputFields_: function() {
      var address = {country: this.getCountrySwitcher_().value};

      var lists = this.getLists_();
      for (var i = 0; i < lists.length; i++) {
        address[lists[i].getAttribute('field')] =
            lists[i].dataModel.slice(0, lists[i].dataModel.length - 1);
      }

      var fields = this.getTextFields_();
      for (var i = 0; i < fields.length; i++) {
        address[fields[i].getAttribute('field')] = fields[i].value;
      }

      return address;
    },

    /**
     * Sets the value of each input field according to |address|.
     * @param {object} address The object with values to use.
     * @private
     */
    setInputFields_: function(address) {
      this.getCountrySwitcher_().value = address.country || '';

      var lists = this.getLists_();
      for (var i = 0; i < lists.length; i++) {
        this.setMultiValueList_(
            lists[i], address[lists[i].getAttribute('field')] || []);
      }

      var fields = this.getTextFields_();
      for (var i = 0; i < fields.length; i++) {
        fields[i].value = address[fields[i].getAttribute('field')] || '';
      }
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the Autofill handler.
     * @private
     */
    saveAddress_: function() {
      var inputFields = this.getInputFields_();
      var address = [
        this.guid_,
        inputFields.fullName || [],
        inputFields.companyName || '',
        inputFields.addrLines || '',
        inputFields.dependentLocality || '',
        inputFields.city || '',
        inputFields.state || '',
        inputFields.postalCode || '',
        inputFields.sortingCode || '',
        inputFields.country || '',
        inputFields.phone || [],
        inputFields.email || [],
        this.languageCode_,
      ];
      chrome.send('setAddress', address);
    },

    /**
     * Connects each input field to the inputFieldChanged_() method that enables
     * or disables the 'Ok' button based on whether all the fields are empty or
     * not.
     * @private
     */
    connectInputEvents_: function() {
      var fields = this.getTextFields_();
      for (var i = 0; i < fields.length; i++) {
        fields[i].oninput = this.inputFieldChanged_.bind(this);
      }
    },

    /**
     * Disables the 'Ok' button if all of the fields are empty.
     * @private
     */
    inputFieldChanged_: function() {
      var disabled = !this.getCountrySwitcher_().value;
      if (disabled) {
        // Length of lists are tested for > 1 due to the "add" placeholder item
        // in the list.
        var lists = this.getLists_();
        for (var i = 0; i < lists.length; i++) {
          if (lists[i].items.length > 1) {
            disabled = false;
            break;
          }
        }
      }

      if (disabled) {
        var fields = this.getTextFields_();
        for (var i = 0; i < fields.length; i++) {
          if (fields[i].value) {
            disabled = false;
            break;
          }
        }
      }

      $('autofill-edit-address-apply-button').disabled = disabled;
    },

    /**
     * Updates the address fields appropriately for the selected country.
     * @private
     */
    countryChanged_: function() {
      var countryCode = this.getCountrySwitcher_().value;
      if (countryCode)
        chrome.send('loadAddressEditorComponents', [countryCode]);
      else
        this.inputFieldChanged_();
    },

    /**
     * Populates the country <select> list.
     * @private
     */
    populateCountryList_: function() {
      var countryList = loadTimeData.getValue('autofillCountrySelectList');

      // Add the countries to the country <select> list.
      var countrySelect = this.getCountrySwitcher_();
      // Add an empty option.
      countrySelect.appendChild(new Option('', ''));
      for (var i = 0; i < countryList.length; i++) {
        var option = new Option(countryList[i].name,
                                countryList[i].value);
        option.disabled = countryList[i].value == 'separator';
        countrySelect.appendChild(option);
      }
    },

    /**
     * Loads the address data from |address|, sets the input fields based on
     * this data, and stores the GUID and language code of the address.
     * @param {!Object} address Lots of info about an address from the browser.
     * @private
     */
    loadAddress_: function(address) {
      this.rebuildInputFields_(address.components);
      this.setInputFields_(address);
      this.inputFieldChanged_();
      this.connectInputEvents_();
      this.guid_ = address.guid;
      this.languageCode_ = address.languageCode;
    },

    /**
     * Takes a snapshot of the input values, clears the input values, loads the
     * address input layout from |input.components|, restores the input values
     * from snapshot, and stores the |input.languageCode| for the address.
     * @param {{languageCode: string, components: Array.<Array.<Object>>}} input
     *     Info about how to layout inputs fields in this dialog.
     * @private
     */
    loadAddressComponents_: function(input) {
      var inputFields = this.getInputFields_();
      for (var fieldName in inputFields) {
        if (inputFields.hasOwnProperty(fieldName))
          this.savedFieldValues_[fieldName] = inputFields[fieldName];
      }
      this.rebuildInputFields_(input.components);
      this.setInputFields_(this.savedFieldValues_);
      this.inputFieldChanged_();
      this.connectInputEvents_();
      this.languageCode_ = input.languageCode;
    },

    /**
     * Clears address inputs and rebuilds the input fields according to
     * |components|.
     * @param {Array.<Array.<Object>>} components A list of information about
     *     each input field.
     * @private
     */
    rebuildInputFields_: function(components) {
      var content = $('autofill-edit-address-fields');
      content.innerHTML = '';

      var customContainerElements = {fullName: 'div'};
      var customInputElements = {fullName: 'list', addrLines: 'textarea'};

      for (var i in components) {
        var row = document.createElement('div');
        row.classList.add('input-group', 'settings-row');
        content.appendChild(row);

        for (var j in components[i]) {
          if (components[i][j].field == 'country')
            continue;

          var fieldContainer = document.createElement(
              customContainerElements[components[i][j].field] || 'label');
          row.appendChild(fieldContainer);

          var fieldName = document.createElement('div');
          fieldName.textContent = components[i][j].name;
          fieldContainer.appendChild(fieldName);

          var input = document.createElement(
              customInputElements[components[i][j].field] || 'input');
          input.setAttribute('field', components[i][j].field);
          input.classList.add(components[i][j].length);
          input.setAttribute('placeholder', components[i][j].placeholder || '');
          fieldContainer.appendChild(input);

          if (input.tagName == 'LIST') {
            options.autofillOptions.AutofillValuesList.decorate(input);
            input.autoExpands = true;
          }
        }
      }
    },
  };

  AutofillEditAddressOverlay.loadAddress = function(address) {
    AutofillEditAddressOverlay.getInstance().loadAddress_(address);
  };

  AutofillEditAddressOverlay.loadAddressComponents = function(input) {
    AutofillEditAddressOverlay.getInstance().loadAddressComponents_(input);
  };

  AutofillEditAddressOverlay.setTitle = function(title) {
    $('autofill-address-title').textContent = title;
  };

  AutofillEditAddressOverlay.setValidatedPhoneNumbers = function(numbers) {
    var instance = AutofillEditAddressOverlay.getInstance();
    var phoneList = instance.pageDiv.querySelector('[field=phone]');
    instance.setMultiValueList_(phoneList, numbers);
    phoneList.didReceiveValidationResult();
  };

  // Export
  return {
    AutofillEditAddressOverlay: AutofillEditAddressOverlay
  };
});
