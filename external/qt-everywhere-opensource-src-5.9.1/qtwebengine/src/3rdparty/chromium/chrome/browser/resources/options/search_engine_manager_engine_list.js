// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{canBeDefault: boolean,
 *            canBeEdited: boolean,
 *            canBeRemoved: boolean,
 *            default: boolean,
 *            displayName: string,
 *            extension: (Object|undefined),
 *            iconURL: (string|undefined),
 *            isOmniboxExtension: boolean,
 *            keyword: string,
 *            modelIndex: string,
 *            name: string,
 *            url: string,
 *            urlLocked: boolean}}
 * @see chrome/browser/ui/webui/options/search_engine_manager_handler.cc
 */
var SearchEngine;

cr.define('options.search_engines', function() {
  /** @const */ var ControlledSettingIndicator =
                    options.ControlledSettingIndicator;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var ListSelectionController = cr.ui.ListSelectionController;

  /**
   * Creates a new search engine list item.
   * @param {SearchEngine} searchEngine The search engine this represents.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function SearchEngineListItem(searchEngine) {
    var el = cr.doc.createElement('div');
    el.searchEngine_ = searchEngine;
    SearchEngineListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a search engine list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  SearchEngineListItem.decorate = function(el) {
    el.__proto__ = SearchEngineListItem.prototype;
    el.decorate();
  };

  SearchEngineListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /**
     * Input field for editing the engine name.
     * @type {HTMLElement}
     * @private
     */
    nameField_: null,

    /**
     * Input field for editing the engine keyword.
     * @type {HTMLElement}
     * @private
     */
    keywordField_: null,

    /**
     * Input field for editing the engine url.
     * @type {HTMLElement}
     * @private
     */
    urlField_: null,

    /**
     * Whether or not an input validation request is currently outstanding.
     * @type {boolean}
     * @private
     */
    waitingForValidation_: false,

    /**
     * Whether or not the current set of input is known to be valid.
     * @type {boolean}
     * @private
     */
    currentlyValid_: false,

    /**
     * @type {?SearchEngine}
     */
    searchEngine_: null,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      var engine = this.searchEngine_;

      if (engine.modelIndex == '-1') {
        this.isPlaceholder = true;
        engine.name = '';
        engine.keyword = '';
        engine.url = '';
      }

      this.currentlyValid_ = !this.isPlaceholder;

      if (engine.default)
        this.classList.add('default');

      this.deletable = engine.canBeRemoved;
      this.closeButtonFocusAllowed = true;

      // Construct the name column.
      var nameColEl = this.ownerDocument.createElement('div');
      nameColEl.className = 'name-column';
      nameColEl.classList.add('weakrtl');
      this.contentElement.appendChild(nameColEl);

      // Add the favicon.
      var faviconDivEl = this.ownerDocument.createElement('div');
      faviconDivEl.className = 'favicon';
      if (!this.isPlaceholder) {
        // Force default icon if no iconURL is available.
        faviconDivEl.style.backgroundImage =
            cr.icon.getFavicon(engine.iconURL || '');
      }

      nameColEl.appendChild(faviconDivEl);

      var nameEl = this.createEditableTextCell(engine.displayName);
      nameEl.classList.add('weakrtl');
      nameColEl.appendChild(nameEl);

      // Then the keyword column.
      var keywordEl = this.createEditableTextCell(engine.keyword);
      keywordEl.className = 'keyword-column';
      keywordEl.classList.add('weakrtl');
      this.contentElement.appendChild(keywordEl);

      // And the URL column.
      var urlEl = this.createEditableTextCell(engine.url);
      var makeDefaultButtonEl = null;
      // Extensions should not display a URL column.
      if (!engine.isOmniboxExtension) {
        var urlWithButtonEl = this.ownerDocument.createElement('div');
        urlWithButtonEl.appendChild(urlEl);
        urlWithButtonEl.className = 'url-column';
        urlWithButtonEl.classList.add('weakrtl');
        this.contentElement.appendChild(urlWithButtonEl);
        // Add the Make Default button. Temporary until drag-and-drop
        // re-ordering is implemented. When this is removed, remove the extra
        // div above.
        if (engine.canBeDefault) {
          makeDefaultButtonEl = this.ownerDocument.createElement('button');
          makeDefaultButtonEl.className =
              'custom-appearance list-inline-button';
          makeDefaultButtonEl.textContent =
              loadTimeData.getString('makeDefaultSearchEngineButton');
          makeDefaultButtonEl.onclick = function(e) {
            chrome.send('managerSetDefaultSearchEngine', [engine.modelIndex]);
          };
          makeDefaultButtonEl.onmousedown = function(e) {
            // Don't select the row when clicking the button.
            e.stopPropagation();
            // Don't focus on the button.
            e.preventDefault();
          };
          urlWithButtonEl.appendChild(makeDefaultButtonEl);
        }
      }

      // Do final adjustment to the input fields.
      this.nameField_ = /** @type {HTMLElement} */(
          nameEl.querySelector('input'));
      // The editable field uses the raw name, not the display name.
      this.nameField_.value = engine.name;
      this.keywordField_ = /** @type {HTMLElement} */(
          keywordEl.querySelector('input'));
      this.urlField_ = /** @type {HTMLElement} */(urlEl.querySelector('input'));

      if (engine.urlLocked)
        this.urlField_.disabled = true;

      if (this.isPlaceholder) {
        this.nameField_.placeholder =
            loadTimeData.getString('searchEngineTableNamePlaceholder');
        this.keywordField_.placeholder =
            loadTimeData.getString('searchEngineTableKeywordPlaceholder');
        this.urlField_.placeholder =
            loadTimeData.getString('searchEngineTableURLPlaceholder');
      }

      this.setFocusableColumnIndex(this.nameField_, 0);
      this.setFocusableColumnIndex(this.keywordField_, 1);
      this.setFocusableColumnIndex(this.urlField_, 2);
      this.setFocusableColumnIndex(makeDefaultButtonEl, 3);
      this.setFocusableColumnIndex(this.closeButtonElement, 4);

      var fields = [this.nameField_, this.keywordField_, this.urlField_];
        for (var i = 0; i < fields.length; i++) {
        fields[i].oninput = this.startFieldValidation_.bind(this);
      }

      // Listen for edit events.
      if (engine.canBeEdited) {
        this.addEventListener('edit', this.onEditStarted_.bind(this));
        this.addEventListener('canceledit', this.onEditCancelled_.bind(this));
        this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
      } else {
        this.editable = false;
        this.querySelector('.row-delete-button').hidden = true;
        var indicator = new ControlledSettingIndicator();
        indicator.setAttribute('setting', 'search-engine');
        // Create a synthetic pref change event decorated as
        // CoreOptionsHandler::CreateValueForPref() does.
        var event = new Event(this.contentType);
        if (engine.extension) {
          event.value = { controlledBy: 'extension',
                          extension: engine.extension };
        } else {
          event.value = { controlledBy: 'policy' };
        }
        indicator.handlePrefChange(event);
        this.appendChild(indicator);
      }
    },

    /** @override */
    get currentInputIsValid() {
      return !this.waitingForValidation_ && this.currentlyValid_;
    },

    /** @override */
    get hasBeenEdited() {
      var engine = this.searchEngine_;
      return this.nameField_.value != engine.name ||
             this.keywordField_.value != engine.keyword ||
             this.urlField_.value != engine.url;
    },

    /**
     * Called when entering edit mode; starts an edit session in the model.
     * @param {Event} e The edit event.
     * @private
     */
    onEditStarted_: function(e) {
      var editIndex = this.searchEngine_.modelIndex;
      chrome.send('editSearchEngine', [String(editIndex)]);
      this.startFieldValidation_();
    },

    /**
     * Called when committing an edit; updates the model.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      chrome.send('searchEngineEditCompleted', this.getInputFieldValues_());
    },

    /**
     * Called when cancelling an edit; informs the model and resets the control
     * states.
     * @param {Event} e The cancel event.
     * @private
     */
    onEditCancelled_: function(e) {
      chrome.send('searchEngineEditCancelled');

      // The name field has been automatically set to match the display name,
      // but it should use the raw name instead.
      this.nameField_.value = this.searchEngine_.name;
      this.currentlyValid_ = !this.isPlaceholder;
    },

    /**
     * Returns the input field values as an array suitable for passing to
     * chrome.send. The order of the array is important.
     * @private
     * @return {Array} The current input field values.
     */
    getInputFieldValues_: function() {
      return [this.nameField_.value,
              this.keywordField_.value,
              this.urlField_.value];
    },

    /**
     * Begins the process of asynchronously validing the input fields.
     * @private
     */
    startFieldValidation_: function() {
      this.waitingForValidation_ = true;
      var args = this.getInputFieldValues_();
      args.push(this.searchEngine_.modelIndex);
      chrome.send('checkSearchEngineInfoValidity', args);
    },

    /**
     * Callback for the completion of an input validition check.
     * @param {Object} validity A dictionary of validitation results.
     */
    validationComplete: function(validity) {
      this.waitingForValidation_ = false;
      if (validity.name) {
        this.nameField_.setCustomValidity('');
      } else {
        this.nameField_.setCustomValidity(
            loadTimeData.getString('editSearchEngineInvalidTitleToolTip'));
      }

      if (validity.keyword) {
        this.keywordField_.setCustomValidity('');
      } else {
        this.keywordField_.setCustomValidity(
            loadTimeData.getString('editSearchEngineInvalidKeywordToolTip'));
      }

      if (validity.url) {
        this.urlField_.setCustomValidity('');
      } else {
        this.urlField_.setCustomValidity(
            loadTimeData.getString('editSearchEngineInvalidURLToolTip'));
      }

      this.currentlyValid_ = validity.name && validity.keyword && validity.url;
    },
  };

  /**
   * @constructor
   * @extends {options.InlineEditableItemList}
   */
  var SearchEngineList = cr.ui.define('list');

  SearchEngineList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * @override
     * @param {SearchEngine} searchEngine
     */
    createItem: function(searchEngine) {
      return new SearchEngineListItem(searchEngine);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var modelIndex = this.dataModel.item(index).modelIndex;
      chrome.send('removeSearchEngine', [String(modelIndex)]);
    },

    /**
     * Passes the results of an input validation check to the requesting row
     * if it's still being edited.
     * @param {number} modelIndex The model index of the item that was checked.
     * @param {Object} validity A dictionary of validitation results.
     */
    validationComplete: function(validity, modelIndex) {
      // If it's not still being edited, it no longer matters.
      var currentSelection = this.selectedItem;
      if (!currentSelection)
        return;
      var listItem = this.getListItem(currentSelection);
      if (listItem.editing && currentSelection.modelIndex == modelIndex)
        listItem.validationComplete(validity);
    },
  };

  // Export
  return {
    SearchEngineList: SearchEngineList
  };

});

