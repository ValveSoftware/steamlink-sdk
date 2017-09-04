// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Creates a new Language list item.
   * @param {Object} languageInfo The information of the language.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function LanguageListItem(languageInfo) {
    var el = cr.doc.createElement('li');
    el.__proto__ = LanguageListItem.prototype;
    el.language_ = languageInfo;
    el.decorate();
    return el;
  };

  LanguageListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * The language code of this language.
     * @type {?string}
     * @private
     */
    languageCode_: null,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var languageCode = this.language_.code;
      var languageOptions = options.LanguageOptions.getInstance();
      this.deletable = languageOptions.languageIsDeletable(languageCode);
      this.languageCode = languageCode;
      this.languageName = cr.doc.createElement('div');
      this.languageName.className = 'language-name';
      this.languageName.dir = this.language_.textDirection;
      this.languageName.textContent = this.language_.displayName;
      this.contentElement.appendChild(this.languageName);
      this.title = this.language_.nativeDisplayName;
      this.draggable = true;
    },
  };

  /**
   * Creates a new language list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var LanguageList = cr.ui.define('list');

  /**
   * Gets information of a language from the given language code.
   * @param {string} languageCode Language code (ex. "fr").
   */
  LanguageList.getLanguageInfoFromLanguageCode = function(languageCode) {
    // Build the language code to language info dictionary at first time.
    if (!this.languageCodeToLanguageInfo_) {
      this.languageCodeToLanguageInfo_ = {};
      var languageList = loadTimeData.getValue('languageList');
      for (var i = 0; i < languageList.length; i++) {
        var languageInfo = languageList[i];
        this.languageCodeToLanguageInfo_[languageInfo.code] = languageInfo;
      }
    }

    return this.languageCodeToLanguageInfo_[languageCode];
  };

  /**
   * Returns true if the given language code is valid.
   * @param {string} languageCode Language code (ex. "fr").
   */
  LanguageList.isValidLanguageCode = function(languageCode) {
    // Having the display name for the language code means that the
    // language code is valid.
    if (LanguageList.getLanguageInfoFromLanguageCode(languageCode)) {
      return true;
    }
    return false;
  };

  LanguageList.prototype = {
    __proto__: DeletableItemList.prototype,

    // The list item being dragged.
    draggedItem: null,
    // The drop position information: "below" or "above".
    dropPos: null,
    // The preference is a CSV string that describes preferred languages
    // in Chrome OS. The language list is used for showing the language
    // list in "Language and Input" options page.
    preferredLanguagesPref: 'settings.language.preferred_languages',
    // The preference is a CSV string that describes accept languages used
    // for content negotiation. To be more precise, the list will be used
    // in "Accept-Language" header in HTTP requests.
    acceptLanguagesPref: 'intl.accept_languages',

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.selectionModel = new ListSingleSelectionModel;

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', this.redraw.bind(this));

      // Listen to pref change.
      if (cr.isChromeOS) {
        Preferences.getInstance().addEventListener(this.preferredLanguagesPref,
            this.handlePreferredLanguagesPrefChange_.bind(this));
      } else {
        Preferences.getInstance().addEventListener(this.acceptLanguagesPref,
            this.handleAcceptLanguagesPrefChange_.bind(this));
      }

      // Listen to drag and drop events.
      this.addEventListener('dragstart', this.handleDragStart_.bind(this));
      this.addEventListener('dragenter', this.handleDragEnter_.bind(this));
      this.addEventListener('dragover', this.handleDragOver_.bind(this));
      this.addEventListener('drop', this.handleDrop_.bind(this));
      this.addEventListener('dragleave', this.handleDragLeave_.bind(this));
    },

    /**
     * @override
     * @param {string} languageCode
     */
    createItem: function(languageCode) {
      var languageInfo =
          LanguageList.getLanguageInfoFromLanguageCode(languageCode);
      return new LanguageListItem(languageInfo);
    },

    /*
     * For each item, determines whether it's deletable.
     */
    updateDeletable: function() {
      var items = this.items;
      for (var i = 0; i < items.length; ++i) {
        var item = items[i];
        var languageCode = item.languageCode;
        var languageOptions = options.LanguageOptions.getInstance();
        item.deletable = languageOptions.languageIsDeletable(languageCode);
      }
    },

    /**
     * Adds a language to the language list.
     * @param {string} languageCode language code (ex. "fr").
     */
    addLanguage: function(languageCode) {
      // It shouldn't happen but ignore the language code if it's
      // null/undefined, or already present.
      if (!languageCode || this.dataModel.indexOf(languageCode) >= 0) {
        return;
      }
      this.dataModel.push(languageCode);
      // Select the last item, which is the language added.
      this.selectionModel.selectedIndex = this.dataModel.length - 1;

      this.savePreference_();
    },

    /**
     * Gets the language codes of the currently listed languages.
     */
    getLanguageCodes: function() {
      return this.dataModel.slice();
    },

    /**
     * Clears the selection
     */
    clearSelection: function() {
      this.selectionModel.unselectAll();
    },

    /**
     * Gets the language code of the selected language.
     */
    getSelectedLanguageCode: function() {
      return this.selectedItem;
    },

    /**
     * Selects the language by the given language code.
     * @return {boolean} True if the operation is successful.
     */
    selectLanguageByCode: function(languageCode) {
      var index = this.dataModel.indexOf(languageCode);
      if (index >= 0) {
        this.selectionModel.selectedIndex = index;
        return true;
      }
      return false;
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      if (index >= 0) {
        this.dataModel.splice(index, 1);
        // Once the selected item is removed, there will be no selected item.
        // Select the item pointed by the lead index.
        index = this.selectionModel.leadIndex;
        this.savePreference_();
      }
      return index;
    },

    /**
     * Computes the target item of drop event.
     * @param {Event} e The drop or dragover event.
     * @private
     */
    getTargetFromDropEvent_: function(e) {
      var target = e.target;
      // e.target may be an inner element of the list item
      while (target != null && !(target instanceof ListItem)) {
        target = target.parentNode;
      }
      return target;
    },

    /**
     * Handles the dragstart event.
     * @param {Event} e The dragstart event.
     * @private
     */
    handleDragStart_: function(e) {
      var target = e.target;
      // ListItem should be the only draggable element type in the page,
      // but just in case.
      if (target instanceof ListItem) {
        this.draggedItem = target;
        e.dataTransfer.effectAllowed = 'move';
        // We need to put some kind of data in the drag or it will be
        // ignored.  Use the display name in case the user drags to a text
        // field or the desktop.
        e.dataTransfer.setData('text/plain', target.title);
      }
    },

    /**
     * Handles the dragenter event.
     * @param {Event} e The dragenter event.
     * @private
     */
    handleDragEnter_: function(e) {
      e.preventDefault();
    },

    /**
     * Handles the dragover event.
     * @param {Event} e The dragover event.
     * @private
     */
    handleDragOver_: function(e) {
      var dropTarget = this.getTargetFromDropEvent_(e);
      // Determines whether the drop target is to accept the drop.
      // The drop is only successful on another ListItem.
      if (!(dropTarget instanceof ListItem) ||
          dropTarget == this.draggedItem) {
        this.hideDropMarker_();
        return;
      }
      // Compute the drop postion. Should we move the dragged item to
      // below or above the drop target?
      var rect = dropTarget.getBoundingClientRect();
      var dy = e.clientY - rect.top;
      var yRatio = dy / rect.height;
      var dropPos = yRatio <= .5 ? 'above' : 'below';
      this.dropPos = dropPos;
      this.showDropMarker_(dropTarget, dropPos);
      e.preventDefault();
    },

    /**
     * Handles the drop event.
     * @param {Event} e The drop event.
     * @private
     */
    handleDrop_: function(e) {
      var dropTarget = this.getTargetFromDropEvent_(e);
      this.hideDropMarker_();

      // Delete the language from the original position.
      var languageCode = this.draggedItem.languageCode;
      var originalIndex = this.dataModel.indexOf(languageCode);
      this.dataModel.splice(originalIndex, 1);
      // Insert the language to the new position.
      var newIndex = this.dataModel.indexOf(dropTarget.languageCode);
      if (this.dropPos == 'below')
        newIndex += 1;
      this.dataModel.splice(newIndex, 0, languageCode);
      // The cursor should move to the moved item.
      this.selectionModel.selectedIndex = newIndex;
      // Save the preference.
      this.savePreference_();
    },

    /**
     * Handles the dragleave event.
     * @param {Event} e The dragleave event
     * @private
     */
    handleDragLeave_: function(e) {
      this.hideDropMarker_();
    },

    /**
     * Shows and positions the marker to indicate the drop target.
     * @param {HTMLElement} target The current target list item of drop
     * @param {string} pos 'below' or 'above'
     * @private
     */
    showDropMarker_: function(target, pos) {
      window.clearTimeout(this.hideDropMarkerTimer_);
      var marker = $('language-options-list-dropmarker');
      var rect = target.getBoundingClientRect();
      var markerHeight = 8;
      if (pos == 'above') {
        marker.style.top = (rect.top - markerHeight / 2) + 'px';
      } else {
        marker.style.top = (rect.bottom - markerHeight / 2) + 'px';
      }
      marker.style.width = rect.width + 'px';
      marker.style.left = rect.left + 'px';
      marker.style.display = 'block';
    },

    /**
     * Hides the drop marker.
     * @private
     */
    hideDropMarker_: function() {
      // Hide the marker in a timeout to reduce flickering as we move between
      // valid drop targets.
      window.clearTimeout(this.hideDropMarkerTimer_);
      this.hideDropMarkerTimer_ = window.setTimeout(function() {
        $('language-options-list-dropmarker').style.display = '';
      }, 100);
    },

    /**
     * Handles preferred languages pref change.
     * @param {Event} e The change event object.
     * @private
     */
    handlePreferredLanguagesPrefChange_: function(e) {
      var languageCodesInCsv = e.value.value;
      var languageCodes = languageCodesInCsv.split(',');

      // Add the UI language to the initial list of languages.  This is to avoid
      // a bug where the UI language would be removed from the preferred
      // language list by sync on first login.
      // See: crosbug.com/14283
      languageCodes.push(navigator.language);
      languageCodes = this.filterBadLanguageCodes_(languageCodes);
      this.load_(languageCodes);
    },

    /**
     * Handles accept languages pref change.
     * @param {Event} e The change event object.
     * @private
     */
    handleAcceptLanguagesPrefChange_: function(e) {
      var languageCodesInCsv = e.value.value;
      var languageCodes = this.filterBadLanguageCodes_(
          languageCodesInCsv.split(','));
      this.load_(languageCodes);
    },

    /**
     * Loads given language list.
     * @param {!Array} languageCodes List of language codes.
     * @private
     */
    load_: function(languageCodes) {
      // Preserve the original selected index. See comments below.
      var originalSelectedIndex = this.selectionModel.selectedIndex;
      this.dataModel = new ArrayDataModel(languageCodes);
      if (originalSelectedIndex >= 0 &&
          originalSelectedIndex < this.dataModel.length) {
        // Restore the original selected index if the selected index is
        // valid after the data model is loaded. This is needed to keep
        // the selected language after the languge is added or removed.
        this.selectionModel.selectedIndex = originalSelectedIndex;
        // The lead index should be updated too.
        this.selectionModel.leadIndex = originalSelectedIndex;
      } else if (this.dataModel.length > 0) {
        // Otherwise, select the first item if it's not empty.
        // Note that ListSingleSelectionModel won't select an item
        // automatically, hence we manually select the first item here.
        this.selectionModel.selectedIndex = 0;
      }
    },

    /**
     * Saves the preference.
     */
    savePreference_: function() {
      chrome.send('updateLanguageList', [this.dataModel.slice()]);
      cr.dispatchSimpleEvent(this, 'save');
    },

    /**
     * Filters bad language codes in case bad language codes are
     * stored in the preference. Removes duplicates as well.
     * @param {Array} languageCodes List of language codes.
     * @private
     */
    filterBadLanguageCodes_: function(languageCodes) {
      var filteredLanguageCodes = [];
      var seen = {};
      for (var i = 0; i < languageCodes.length; i++) {
        // Check if the the language code is valid, and not
        // duplicate. Otherwise, skip it.
        if (LanguageList.isValidLanguageCode(languageCodes[i]) &&
            !(languageCodes[i] in seen)) {
          filteredLanguageCodes.push(languageCodes[i]);
          seen[languageCodes[i]] = true;
        }
      }
      return filteredLanguageCodes;
    },
  };

  return {
    LanguageList: LanguageList,
    LanguageListItem: LanguageListItem
  };
});
