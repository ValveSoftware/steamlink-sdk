// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.contentSettings', function() {
  /** @const */ var ControlledSettingIndicator =
                    options.ControlledSettingIndicator;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Returns whether exceptions list for the type is editable.
   *
   * @param {string} contentType The type of the list.
   */
  function isEditableType(contentType) {
    // Exceptions of the following lists are not editable for now.
    return !(contentType == 'location' ||
             contentType == 'media-stream-mic' ||
             contentType == 'media-stream-camera' ||
             contentType == 'midi-sysex' ||
             contentType == 'zoomlevels' ||
             isChosenObjectType(contentType));
  }

  /**
   * Returns whether exceptions of this type represent chosen objects.
   *
   * @param {string} contentType The type of the list.
   */
  function isChosenObjectType(contentType) {
    return contentType == 'usb-devices';
  }

  /**
   * Returns the ID of the column containing values for the given content type.
   *
   * @param {string} contentType The type of the list.
   */
  function valueColumnForContentType(contentType) {
    if (contentType == 'usb-devices')
      return 'exception-usb-device-column';
    if (contentType == 'zoomlevels')
      return 'exception-zoom-column';
    return 'exception-behavior-column';
  }

  /**
   * Creates a new exceptions list item.
   *
   * @param {string} contentType The type of the list.
   * @param {string} mode The browser mode, 'otr' or 'normal'.
   * @param {Object} exception A dictionary that contains the data of the
   *     exception.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function ExceptionsListItem(contentType, mode, exception) {
    var el = cr.doc.createElement('div');
    el.mode = mode;
    el.contentType = contentType;
    el.dataItem = exception;
    el.__proto__ = ExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /**
     * Called when an element is decorated as a list item.
     */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      this.isPlaceholder = !this.pattern;
      var patternCell = this.createEditableTextCell(this.pattern);
      patternCell.className = 'exception-pattern';
      patternCell.classList.add('weakrtl');
      this.contentElement.appendChild(patternCell);
      if (this.pattern)
        this.patternLabel = patternCell.querySelector('.static-text');
      var input = patternCell.querySelector('input');

      // Setting label for display mode. |pattern| will be null for the 'add new
      // exception' row.
      if (this.pattern) {
        var settingLabel = cr.doc.createElement('span');
        settingLabel.textContent = this.settingForDisplay();
        settingLabel.className = 'exception-setting overruleable';
        settingLabel.setAttribute('displaymode', 'static');
        this.contentElement.appendChild(settingLabel);
        this.settingLabel = settingLabel;
      }

      // Setting select element for edit mode.
      var select = cr.doc.createElement('select');
      var optionAllow = cr.doc.createElement('option');
      optionAllow.textContent = loadTimeData.getString('allowException');
      optionAllow.value = 'allow';
      select.appendChild(optionAllow);

      if (this.contentType == 'plugins') {
        var optionDetect = cr.doc.createElement('option');
        optionDetect.textContent = loadTimeData.getString('detectException');
        optionDetect.value = 'detect_important_content';
        select.appendChild(optionDetect);
      }

      if (this.contentType == 'cookies') {
        var optionSession = cr.doc.createElement('option');
        optionSession.textContent = loadTimeData.getString('sessionException');
        optionSession.value = 'session_only';
        select.appendChild(optionSession);
      }

      var optionBlock = cr.doc.createElement('option');
      optionBlock.textContent = loadTimeData.getString('blockException');
      optionBlock.value = 'block';
      select.appendChild(optionBlock);

      if (this.isEmbeddingRule()) {
        this.patternLabel.classList.add('sublabel');
        this.editable = false;
      }

      if (this.setting == 'default') {
        // Items that don't have their own settings (parents of 'embedded on'
        // items) aren't deletable.
        this.deletable = false;
        this.editable = false;
      }

      if (this.contentType != 'zoomlevels' &&
          !isChosenObjectType(this.contentType)) {
        this.addEditField(select, this.settingLabel);
        this.contentElement.appendChild(select);
      }
      select.className = 'exception-setting';
      select.setAttribute('aria-labelledby',
                          valueColumnForContentType(this.contentType));

      if (this.pattern)
        select.setAttribute('displaymode', 'edit');

      if (this.contentType == 'zoomlevels') {
        this.deletable = true;

        var zoomLabel = cr.doc.createElement('span');
        zoomLabel.textContent = this.dataItem.zoom;
        zoomLabel.className = 'exception-setting';
        zoomLabel.setAttribute('displaymode', 'static');
        this.contentElement.appendChild(zoomLabel);
        this.zoomLabel = zoomLabel;
      }

      if (isChosenObjectType(this.contentType) &&
          this.dataItem.object !== undefined) {
        this.deletable = true;

        var objectLabel = cr.doc.createElement('span');
        objectLabel.textContent = this.dataItem['objectName'];
        objectLabel.className = 'exception-setting';
        objectLabel.setAttribute('displaymode', 'static');
        this.contentElement.appendChild(objectLabel);
        this.objectLabel = objectLabel;
      }

      // Used to track whether the URL pattern in the input is valid.
      // This will be true if the browser process has informed us that the
      // current text in the input is valid. Changing the text resets this to
      // false, and getting a response from the browser sets it back to true.
      // It starts off as false for empty string (new exceptions) or true for
      // already-existing exceptions (which we assume are valid).
      this.inputValidityKnown = this.pattern;
      // This one tracks the actual validity of the pattern in the input. This
      // starts off as true so as not to annoy the user when they add a new and
      // empty input.
      this.inputIsValid = true;

      this.input = input;
      this.select = select;

      this.updateEditables();
      this.editable = this.editable && isEditableType(this.contentType);

      // If the source of the content setting exception is not a user
      // preference, that source controls the exception and the user cannot edit
      // or delete it.
      var controlledBy =
          this.dataItem.source && this.dataItem.source != 'preference' ?
              this.dataItem.source : null;

      if (controlledBy) {
        this.setAttribute('controlled-by', controlledBy);
        this.deletable = false;
        this.editable = false;
      }

      if (controlledBy == 'policy' || controlledBy == 'extension') {
        this.querySelector('.row-delete-button').hidden = true;
        this.appendIndicatorElement(controlledBy);
      }

      // If the exception comes from a hosted app, display the name and the
      // icon of the app.
      if (controlledBy == 'HostedApp') {
        this.title =
            loadTimeData.getString('setBy') + ' ' + this.dataItem.appName;
        var button = this.querySelector('.row-delete-button');
        // Use the host app's favicon (16px, match bigger size).
        // See c/b/ui/webui/extensions/extension_icon_source.h
        // for a description of the chrome://extension-icon URL.
        button.style.backgroundImage =
            'url(\'chrome://extension-icon/' + this.dataItem.appId + '/16/1\')';
      }

      var listItem = this;
      // Handle events on the editable nodes.
      input.oninput = function(event) {
        listItem.inputValidityKnown = false;
        chrome.send('checkExceptionPatternValidity',
                    [listItem.contentType, listItem.mode, input.value]);
      };

      // Listen for edit events.
      this.addEventListener('canceledit', this.onEditCancelled_);
      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /**
     * Appends an indicator element to the item. Should be called at most once.
     *
     * @param {string} controlledBy The source that controls the item.
     */
    appendIndicatorElement: function(controlledBy) {
      var indicator = new ControlledSettingIndicator();
      indicator.setAttribute('content-exception', this.contentType);
      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event(this.contentType);
      event.value = {controlledBy: controlledBy};
      indicator.handlePrefChange(event);
      this.appendChild(indicator);
    },

    isEmbeddingRule: function() {
      return this.dataItem.embeddingOrigin &&
          this.dataItem.embeddingOrigin !== this.dataItem.origin;
    },

    /**
     * The pattern (e.g., a URL) for the exception.
     *
     * @type {string}
     */
    get pattern() {
      if (!this.isEmbeddingRule())
        return this.dataItem.origin;

      return loadTimeData.getStringF('embeddedOnHost',
                                     this.dataItem.embeddingOrigin);
    },
    set pattern(pattern) {
      if (!this.editable)
        console.error('Tried to change uneditable pattern');

      this.dataItem.displayPattern = pattern;
    },

    /**
     * The setting (allow/block) for the exception.
     *
     * @type {string}
     */
    get setting() {
      return this.dataItem.setting;
    },
    set setting(setting) {
      this.dataItem.setting = setting;
    },

    /**
     * Gets a human-readable setting string.
     *
     * @return {string} The display string.
     */
    settingForDisplay: function() {
      return this.getDisplayStringForSetting(this.setting);
    },

    /**
     * Gets a human-readable display string for setting.
     *
     * @param {string} setting The setting to be displayed.
     * @return {string} The display string.
     */
    getDisplayStringForSetting: function(setting) {
      if (setting == 'allow')
        return loadTimeData.getString('allowException');
      else if (setting == 'block')
        return loadTimeData.getString('blockException');
      else if (setting == 'ask')
        return loadTimeData.getString('askException');
      else if (setting == 'session_only')
        return loadTimeData.getString('sessionException');
      else if (setting == 'detect_important_content')
        return loadTimeData.getString('detectException');
      else if (setting == 'default')
        return '';

      console.error('Unknown setting: [' + setting + ']');
      return '';
    },

    /**
     * Update this list item to reflect whether the input is a valid pattern.
     *
     * @param {boolean} valid Whether said pattern is valid in the context of a
     *     content exception setting.
     */
    setPatternValid: function(valid) {
      if (valid || !this.input.value)
        this.input.setCustomValidity('');
      else
        this.input.setCustomValidity(' ');
      this.inputIsValid = valid;
      this.inputValidityKnown = true;
    },

    /**
     * Set the <input> to its original contents. Used when the user quits
     * editing.
     */
    resetInput: function() {
      this.input.value = this.pattern;
    },

    /**
     * Copy the data model values to the editable nodes.
     */
    updateEditables: function() {
      this.resetInput();

      var settingOption =
          this.select.querySelector('[value=\'' + this.setting + '\']');
      if (settingOption)
        settingOption.selected = true;
    },

    /**
     * Updates UI to indicate that the exception was overruled by a source.
     *
     * @param {string} overruledBy The source that overrules the exception.
     */
    setOverruledBy: function(overruledBy) {
      this.classList.toggle('overruled', !!overruledBy);
      this.appendIndicatorElement(overruledBy);
    },

    /** @override */
    get currentInputIsValid() {
      return this.inputValidityKnown && this.inputIsValid;
    },

    /** @override */
    get hasBeenEdited() {
      var livePattern = this.input.value;
      var liveSetting = this.select.value;
      return livePattern != this.pattern || liveSetting != this.setting;
    },

    /**
     * Called when committing an edit.
     *
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var newPattern = this.input.value;
      var newSetting = this.select.value;

      this.finishEdit(newPattern, newSetting);
    },

    /**
     * Called when cancelling an edit; resets the control states.
     *
     * @param {Event} e The cancel event.
     * @private
     */
    onEditCancelled_: function(e) {
      this.updateEditables();
      this.setPatternValid(true);
    },

    /**
     * Editing is complete; update the model.
     *
     * @param {string} newPattern The pattern that the user entered.
     * @param {string} newSetting The setting the user chose.
     */
    finishEdit: function(newPattern, newSetting) {
      this.patternLabel.textContent = newPattern;
      this.settingLabel.textContent = this.settingForDisplay();
      var oldPattern = this.pattern;
      this.pattern = newPattern;
      this.setting = newSetting;

      // TODO(estade): this will need to be updated if geolocation/notifications
      // become editable.
      if (oldPattern != newPattern) {
        chrome.send('removeException',
                    [this.contentType, this.mode, oldPattern]);
      }

      chrome.send('setException',
                  [this.contentType, this.mode, newPattern, newSetting]);
    },
  };

  /**
   * Creates a new list item for the Add New Item row, which doesn't represent
   * an actual entry in the exceptions list but allows the user to add new
   * exceptions.
   *
   * @param {string} contentType The type of the list.
   * @param {string} mode The browser mode, 'otr' or 'normal'.
   * @constructor
   * @extends {options.contentSettings.ExceptionsListItem}
   */
  function ExceptionsAddRowListItem(contentType, mode) {
    var el = cr.doc.createElement('div');
    el.mode = mode;
    el.contentType = contentType;
    el.dataItem = [];
    el.__proto__ = ExceptionsAddRowListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsAddRowListItem.prototype = {
    __proto__: ExceptionsListItem.prototype,

    decorate: function() {
      ExceptionsListItem.prototype.decorate.call(this);

      this.input.placeholder =
          loadTimeData.getString('addNewExceptionInstructions');

      // Do we always want a default of allow?
      this.setting = 'allow';
    },

    /**
     * Clear the <input> and let the placeholder text show again.
     */
    resetInput: function() {
      this.input.value = '';
    },

    /** @override */
    get hasBeenEdited() {
      return this.input.value != '';
    },

    /**
     * Editing is complete; update the model. As long as the pattern isn't
     * empty, we'll just add it.
     *
     * @param {string} newPattern The pattern that the user entered.
     * @param {string} newSetting The setting the user chose.
     */
    finishEdit: function(newPattern, newSetting) {
      this.resetInput();
      chrome.send('setException',
                  [this.contentType, this.mode, newPattern, newSetting]);
    },
  };

  /**
   * Creates a new exceptions list.
   *
   * @constructor
   * @extends {options.InlineEditableItemList}
   */
  var ExceptionsList = cr.ui.define('list');

  ExceptionsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * Called when an element is decorated as a list.
     */
    decorate: function() {
      InlineEditableItemList.prototype.decorate.call(this);

      this.classList.add('settings-list');

      for (var parentNode = this.parentNode; parentNode;
           parentNode = parentNode.parentNode) {
        if (parentNode.hasAttribute('contentType')) {
          this.contentType = parentNode.getAttribute('contentType');
          break;
        }
      }

      if (!this.isEditable())
        this.tabIndex = 0;

      this.mode = this.getAttribute('mode');
      this.autoExpands = true;
      this.reset();
    },

    /**
     * Creates an item to go in the list.
     *
     * @param {Object} entry The element from the data model for this row.
     */
    createItem: function(entry) {
      if (entry) {
        return new ExceptionsListItem(this.contentType,
                                      this.mode,
                                      entry);
      } else {
        var addRowItem = new ExceptionsAddRowListItem(this.contentType,
                                                      this.mode);
        addRowItem.deletable = false;
        return addRowItem;
      }
    },

    /**
     * Updates UI to indicate that user exceptions were overruled by a source.
     *
     * @param {string} overruledBy The source that overrules user exceptions.
     */
    setOverruledBy: function(overruledBy) {
      for (var index = 0; index < this.dataModel.length; ++index) {
        var item = this.getListItemByIndex(index);
        if (item.dataItem.source == 'preference')
          item.setOverruledBy(overruledBy);
      }
    },

    /**
     * Sets the exceptions in the js model.
     *
     * @param {Array<options.Exception>} entries A list of dictionaries of
     *     values, each dictionary represents an exception.
     */
    setExceptions: function(entries) {
      var deleteCount = this.dataModel.length;

      if (this.isEditable()) {
        // We don't want to remove the Add New Exception row.
        deleteCount = deleteCount - 1;
      }

      var args = [0, deleteCount];
      args.push.apply(args, entries);
      this.dataModel.splice.apply(this.dataModel, args);
    },

    /**
     * The browser has finished checking a pattern for validity. Update the list
     * item to reflect this.
     *
     * @param {string} pattern The pattern.
     * @param {boolean} valid Whether said pattern is valid in the context of a
     *     content exception setting.
     */
    patternValidityCheckComplete: function(pattern, valid) {
      var listItems = this.items;
      for (var i = 0; i < listItems.length; i++) {
        var listItem = listItems[i];
        // Don't do anything for messages for the item if it is not the intended
        // recipient, or if the response is stale (i.e. the input value has
        // changed since we sent the request to analyze it).
        if (pattern == listItem.input.value)
          listItem.setPatternValid(valid);
      }
    },

    /**
     * Returns whether the rows are editable in this list.
     */
    isEditable: function() {
      // Exceptions of the following lists are not editable for now.
      return isEditableType(this.contentType);
    },

    /**
     * Removes all exceptions from the js model.
     */
    reset: function() {
      if (this.isEditable()) {
        // The null creates the Add New Exception row.
        this.dataModel = new ArrayDataModel([null]);
      } else {
        this.dataModel = new ArrayDataModel([]);
      }
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var listItem = this.getListItemByIndex(index);
      if (!listItem.deletable)
        return;

      var dataItem = listItem.dataItem;
      var params = [listItem.contentType,
                    listItem.mode,
                    dataItem.origin,
                    dataItem.embeddingOrigin];

      if (isChosenObjectType(this.contentType))
        params.push(dataItem.object);

      chrome.send('removeException', params);
    },
  };

  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated handling of content settings list subpage.
   *
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function ContentSettingsExceptionsArea() {
    Page.call(this, 'contentExceptions',
              loadTimeData.getString('contentSettingsPageTabTitle'),
              'content-settings-exceptions-area');
  }

  cr.addSingletonGetter(ContentSettingsExceptionsArea);

  ContentSettingsExceptionsArea.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var exceptionsLists = this.pageDiv.querySelectorAll('list');
      for (var i = 0; i < exceptionsLists.length; i++) {
        options.contentSettings.ExceptionsList.decorate(exceptionsLists[i]);
      }

      ContentSettingsExceptionsArea.hideOTRLists(false);

      // If the user types in the URL without a hash, show just cookies.
      this.showList('cookies');

      $('content-settings-exceptions-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);
    },

    /**
     * Shows one list and hides all others.
     *
     * @param {string} type The content type.
     */
    showList: function(type) {
      // Update the title for the type that was shown.
      this.title = loadTimeData.getString(type + 'TabTitle');

      var header = this.pageDiv.querySelector('h1');
      var camelCasedType = type.replace(/-([a-z])/g, function(g) {
        return g[1].toUpperCase();
      });
      header.textContent = loadTimeData.getString(camelCasedType + 'Header');

      var divs = this.pageDiv.querySelectorAll('div[contentType]');
      for (var i = 0; i < divs.length; i++) {
        if (divs[i].getAttribute('contentType') == type)
          divs[i].hidden = false;
        else
          divs[i].hidden = true;
      }

      var valueColumnId = valueColumnForContentType(type);
      var headers =
          this.pageDiv.querySelectorAll('div.exception-value-column-header');
      for (var i = 0; i < headers.length; ++i)
        headers[i].hidden = (headers[i].id != valueColumnId);
    },

    /**
     * Called after the page has been shown. Show the content type for the
     * location's hash.
     */
    didShowPage: function() {
      if (this.hash)
        this.showList(this.hash.slice(1));
    },
  };

  /**
   * Called when the last incognito window is closed.
   */
  ContentSettingsExceptionsArea.OTRProfileDestroyed = function() {
    this.hideOTRLists(true);
  };

  /**
   * Hides the incognito exceptions lists and optionally clears them as well.
   * @param {boolean} clear Whether to clear the lists.
   */
  ContentSettingsExceptionsArea.hideOTRLists = function(clear) {
    var otrLists = document.querySelectorAll('list[mode=otr]');

    for (var i = 0; i < otrLists.length; i++) {
      otrLists[i].parentNode.hidden = true;
      if (clear)
        otrLists[i].reset();
    }
  };

  return {
    ExceptionsListItem: ExceptionsListItem,
    ExceptionsAddRowListItem: ExceptionsAddRowListItem,
    ExceptionsList: ExceptionsList,
    ContentSettingsExceptionsArea: ContentSettingsExceptionsArea,
  };
});
