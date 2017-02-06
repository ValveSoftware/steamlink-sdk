// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_error.js">

cr.define('extensions', function() {
  'use strict';

  var ExtensionType = chrome.developerPrivate.ExtensionType;

  /**
   * @param {string} name The name of the template to clone.
   * @return {!Element} The freshly cloned template.
   */
  function cloneTemplate(name) {
    var node = $('templates').querySelector('.' + name).cloneNode(true);
    return assertInstanceof(node, Element);
  }

  /**
   * @extends {HTMLElement}
   * @constructor
   */
  function ExtensionWrapper() {
    var wrapper = cloneTemplate('extension-list-item-wrapper');
    wrapper.__proto__ = ExtensionWrapper.prototype;
    wrapper.initialize();
    return wrapper;
  }

  ExtensionWrapper.prototype = {
    __proto__: HTMLElement.prototype,

    initialize: function() {
      var boundary = $('extension-settings-list');
      /** @private {!extensions.FocusRow} */
      this.focusRow_ = new extensions.FocusRow(this, boundary);
    },

    /** @return {!cr.ui.FocusRow} */
    getFocusRow: function() {
      return this.focusRow_;
    },

    /**
     * Add an item to the focus row and listen for |eventType| events.
     * @param {string} focusType A tag used to identify equivalent elements when
     *     changing focus between rows.
     * @param {string} query A query to select the element to set up.
     * @param {string=} opt_eventType The type of event to listen to.
     * @param {function(Event)=} opt_handler The function that should be called
     *     by the event.
     * @private
     */
    setupColumn: function(focusType, query, opt_eventType, opt_handler) {
      assert(this.focusRow_.addItem(focusType, query));
      if (opt_eventType) {
        assert(opt_handler);
        this.querySelector(query).addEventListener(opt_eventType, opt_handler);
      }
    },
  };

  var ExtensionCommandsOverlay = extensions.ExtensionCommandsOverlay;

  /**
   * Compares two extensions for the order they should appear in the list.
   * @param {chrome.developerPrivate.ExtensionInfo} a The first extension.
   * @param {chrome.developerPrivate.ExtensionInfo} b The second extension.
   * returns {number} -1 if A comes before B, 1 if A comes after B, 0 if equal.
   */
  function compareExtensions(a, b) {
    function compare(x, y) {
      return x < y ? -1 : (x > y ? 1 : 0);
    }
    function compareLocation(x, y) {
      if (x.location == y.location)
        return 0;
      if (x.location == chrome.developerPrivate.Location.UNPACKED)
        return -1;
      if (y.location == chrome.developerPrivate.Location.UNPACKED)
        return 1;
      return 0;
    }
    return compareLocation(a, b) ||
           compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
           compare(a.id, b.id);
  }

  /** @interface */
  function ExtensionListDelegate() {}

  ExtensionListDelegate.prototype = {
    /**
     * Called when the number of extensions in the list has changed.
     */
    onExtensionCountChanged: assertNotReached,
  };

  /**
   * Creates a new list of extensions.
   * @param {extensions.ExtensionListDelegate} delegate
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionList(delegate) {
    var div = document.createElement('div');
    div.__proto__ = ExtensionList.prototype;
    div.initialize(delegate);
    return div;
  }

  ExtensionList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Indicates whether an embedded options page that was navigated to through
     * the '?options=' URL query has been shown to the user. This is necessary
     * to prevent showExtensionNodes_ from opening the options more than once.
     * @type {boolean}
     * @private
     */
    optionsShown_: false,

    /** @private {!cr.ui.FocusGrid} */
    focusGrid_: new cr.ui.FocusGrid(),

    /**
     * Indicates whether an uninstall dialog is being shown to prevent multiple
     * dialogs from being displayed.
     * @private {boolean}
     */
    uninstallIsShowing_: false,

    /**
     * Indicates whether a permissions prompt is showing.
     * @private {boolean}
     */
    permissionsPromptIsShowing_: false,

    /**
     * Whether or not any initial navigation (like scrolling to an extension,
     * or opening an options page) has occurred.
     * @private {boolean}
     */
    didInitialNavigation_: false,

    /**
     * Whether or not incognito mode is available.
     * @private {boolean}
     */
    incognitoAvailable_: false,

    /**
     * Whether or not the app info dialog is enabled.
     * @private {boolean}
     */
    enableAppInfoDialog_: false,

    /**
     * Initializes the list.
     * @param {!extensions.ExtensionListDelegate} delegate
     */
    initialize: function(delegate) {
      /** @private {!Array<chrome.developerPrivate.ExtensionInfo>} */
      this.extensions_ = [];

      /** @private {!extensions.ExtensionListDelegate} */
      this.delegate_ = delegate;

      this.resetLoadFinished();

      chrome.developerPrivate.onItemStateChanged.addListener(
          function(eventData) {
        var EventType = chrome.developerPrivate.EventType;
        switch (eventData.event_type) {
          case EventType.VIEW_REGISTERED:
          case EventType.VIEW_UNREGISTERED:
          case EventType.INSTALLED:
          case EventType.LOADED:
          case EventType.UNLOADED:
          case EventType.ERROR_ADDED:
          case EventType.ERRORS_REMOVED:
          case EventType.PREFS_CHANGED:
            if (eventData.extensionInfo) {
              this.updateOrCreateWrapper_(eventData.extensionInfo);
              this.focusGrid_.ensureRowActive();
            }
            break;
          case EventType.UNINSTALLED:
            var index = this.getIndexOfExtension_(eventData.item_id);
            this.extensions_.splice(index, 1);
            this.removeWrapper_(getRequiredElement(eventData.item_id));
            break;
          default:
            assertNotReached();
        }

        if (eventData.event_type == EventType.UNLOADED)
          this.hideEmbeddedExtensionOptions_(eventData.item_id);

        if (eventData.event_type == EventType.INSTALLED ||
            eventData.event_type == EventType.UNINSTALLED) {
          this.delegate_.onExtensionCountChanged();
        }

        if (eventData.event_type == EventType.LOADED ||
            eventData.event_type == EventType.UNLOADED ||
            eventData.event_type == EventType.PREFS_CHANGED ||
            eventData.event_type == EventType.UNINSTALLED) {
          // We update the commands overlay whenever an extension is added or
          // removed (other updates wouldn't affect command-ly things). We
          // need both UNLOADED and UNINSTALLED since the UNLOADED event results
          // in an extension losing active keybindings, and UNINSTALLED can
          // result in the "Keyboard shortcuts" link being removed.
          ExtensionCommandsOverlay.updateExtensionsData(this.extensions_);
        }
      }.bind(this));
    },

    /**
     * Resets the |loadFinished| promise so that it can be used again; this
     * is useful if the page updates and tests need to wait for it to finish.
     */
    resetLoadFinished: function() {
      /**
       * |loadFinished| should be used for testing purposes and will be
       * fulfilled when this list has finished loading the first time.
       * @type {Promise}
       * */
      this.loadFinished = new Promise(function(resolve, reject) {
        /** @private {function(?)} */
        this.resolveLoadFinished_ = resolve;
      }.bind(this));
    },

    /**
     * Updates the extensions on the page.
     * @param {boolean} incognitoAvailable Whether or not incognito is allowed.
     * @param {boolean} enableAppInfoDialog Whether or not the app info dialog
     *     is enabled.
     * @return {Promise} A promise that is resolved once the extensions data is
     *     fully updated.
     */
    updateExtensionsData: function(incognitoAvailable, enableAppInfoDialog) {
      // If we start to need more information about the extension configuration,
      // consider passing in the full object from the ExtensionSettings.
      this.incognitoAvailable_ = incognitoAvailable;
      this.enableAppInfoDialog_ = enableAppInfoDialog;
      /** @private {Promise} */
      this.extensionsUpdated_ = new Promise(function(resolve, reject) {
        chrome.developerPrivate.getExtensionsInfo(
            {includeDisabled: true, includeTerminated: true},
            function(extensions) {
          // Sort in order of unpacked vs. packed, followed by name, followed by
          // id.
          extensions.sort(compareExtensions);
          this.extensions_ = extensions;
          this.showExtensionNodes_();

          // We keep the commands overlay's extension info in sync, so that we
          // don't duplicate the same querying logic there.
          ExtensionCommandsOverlay.updateExtensionsData(this.extensions_);

          resolve();

          // |resolve| is async so it's necessary to use |then| here in order to
          // do work after other |then|s have finished. This is important so
          // elements are visible when these updates happen.
          this.extensionsUpdated_.then(function() {
            this.onUpdateFinished_();
            this.resolveLoadFinished_();
          }.bind(this));
        }.bind(this));
      }.bind(this));
      return this.extensionsUpdated_;
    },

    /**
     * Updates elements that need to be visible in order to update properly.
     * @private
     */
    onUpdateFinished_: function() {
      // Cannot focus or highlight a extension if there are none, and we should
      // only scroll to a particular extension or open the options page once.
      if (this.extensions_.length == 0 || this.didInitialNavigation_)
        return;

      this.didInitialNavigation_ = true;
      assert(!this.hidden);
      assert(!this.parentElement.hidden);

      var idToHighlight = this.getIdQueryParam_();
      if (idToHighlight) {
        var wrapper = $(idToHighlight);
        if (wrapper) {
          this.scrollToWrapper_(idToHighlight);

          var focusRow = wrapper.getFocusRow();
          (focusRow.getFirstFocusable('enabled') ||
           focusRow.getFirstFocusable('remove-enterprise') ||
           focusRow.getFirstFocusable('website') ||
           focusRow.getFirstFocusable('details')).focus();
        }
      }

      var idToOpenOptions = this.getOptionsQueryParam_();
      if (idToOpenOptions && $(idToOpenOptions))
        this.showEmbeddedExtensionOptions_(idToOpenOptions, true);
    },

    /** @return {number} The number of extensions being displayed. */
    getNumExtensions: function() {
      return this.extensions_.length;
    },

    /**
     * @param {string} id The id of the extension.
     * @return {number} The index of the extension with the given id.
     * @private
     */
    getIndexOfExtension_: function(id) {
      for (var i = 0; i < this.extensions_.length; ++i) {
        if (this.extensions_[i].id == id)
          return i;
      }
      return -1;
    },

    getIdQueryParam_: function() {
      return parseQueryParams(document.location)['id'];
    },

    getOptionsQueryParam_: function() {
      return parseQueryParams(document.location)['options'];
    },

    /**
     * Creates or updates all extension items from scratch.
     * @private
     */
    showExtensionNodes_: function() {
      // Any node that is not updated will be removed.
      var seenIds = [];

      // Iterate over the extension data and add each item to the list.
      this.extensions_.forEach(function(extension) {
        seenIds.push(extension.id);
        this.updateOrCreateWrapper_(extension);
      }, this);
      this.focusGrid_.ensureRowActive();

      // Remove extensions that are no longer installed.
      var wrappers = document.querySelectorAll(
          '.extension-list-item-wrapper[id]');
      Array.prototype.forEach.call(wrappers, function(wrapper) {
        if (seenIds.indexOf(wrapper.id) < 0)
          this.removeWrapper_(wrapper);
      }, this);
    },

    /**
     * Removes the wrapper from the DOM and updates the focused element if
     * needed.
     * @param {!Element} wrapper
     * @private
     */
    removeWrapper_: function(wrapper) {
      // If focus is in the wrapper about to be removed, move it first. This
      // happens when clicking the trash can to remove an extension.
      if (wrapper.contains(document.activeElement)) {
        var wrappers = document.querySelectorAll(
            '.extension-list-item-wrapper[id]');
        var index = Array.prototype.indexOf.call(wrappers, wrapper);
        assert(index != -1);
        var focusableWrapper = wrappers[index + 1] || wrappers[index - 1];
        if (focusableWrapper) {
          var newFocusRow = focusableWrapper.getFocusRow();
          newFocusRow.getEquivalentElement(document.activeElement).focus();
        }
      }

      var focusRow = wrapper.getFocusRow();
      this.focusGrid_.removeRow(focusRow);
      this.focusGrid_.ensureRowActive();
      focusRow.destroy();

      wrapper.parentNode.removeChild(wrapper);
    },

    /**
     * Scrolls the page down to the extension node with the given id.
     * @param {string} extensionId The id of the extension to scroll to.
     * @private
     */
    scrollToWrapper_: function(extensionId) {
      // Scroll offset should be calculated slightly higher than the actual
      // offset of the element being scrolled to, so that it ends up not all
      // the way at the top. That way it is clear that there are more elements
      // above the element being scrolled to.
      var wrapper = $(extensionId);
      var scrollFudge = 1.2;
      var scrollTop = wrapper.offsetTop - scrollFudge * wrapper.clientHeight;
      setScrollTopForDocument(document, scrollTop);
    },

    /**
     * Synthesizes and initializes an HTML element for the extension metadata
     * given in |extension|.
     * @param {!chrome.developerPrivate.ExtensionInfo} extension A dictionary
     *     of extension metadata.
     * @param {?Element} nextWrapper The newly created wrapper will be inserted
     *     before |nextWrapper| if non-null (else it will be appended to the
     *     wrapper list).
     * @private
     */
    createWrapper_: function(extension, nextWrapper) {
      var wrapper = new ExtensionWrapper;
      wrapper.id = extension.id;

      // The 'Permissions' link.
      wrapper.setupColumn('details', '.permissions-link', 'click', function(e) {
        if (!this.permissionsPromptIsShowing_) {
          chrome.developerPrivate.showPermissionsDialog(extension.id,
                                                        function() {
            this.permissionsPromptIsShowing_ = false;
          }.bind(this));
          this.permissionsPromptIsShowing_ = true;
        }
        e.preventDefault();
      });

      wrapper.setupColumn('options', '.options-button', 'click', function(e) {
        this.showEmbeddedExtensionOptions_(extension.id, false);
        e.preventDefault();
      }.bind(this));

      // The 'Options' button or link, depending on its behaviour.
      // Set an href to get the correct mouse-over appearance (link,
      // footer) - but the actual link opening is done through developerPrivate
      // API with a preventDefault().
      wrapper.querySelector('.options-link').href =
          extension.optionsPage ? extension.optionsPage.url : '';
      wrapper.setupColumn('options', '.options-link', 'click', function(e) {
        chrome.developerPrivate.showOptions(extension.id);
        e.preventDefault();
      });

      // The 'View in Web Store/View Web Site' link.
      wrapper.setupColumn('website', '.site-link');

      // The 'Launch' link.
      wrapper.setupColumn('launch', '.launch-link', 'click', function(e) {
        chrome.management.launchApp(extension.id);
      });

      // The 'Reload' link.
      wrapper.setupColumn('localReload', '.reload-link', 'click', function(e) {
        chrome.developerPrivate.reload(extension.id, {failQuietly: true});
      });

      wrapper.setupColumn('errors', '.errors-link', 'click', function(e) {
        var extensionId = extension.id;
        assert(this.extensions_.length > 0);
        var newEx = this.extensions_.filter(function(e) {
          return e.state == chrome.developerPrivate.ExtensionState.ENABLED &&
              e.id == extensionId;
        })[0];
        var errors = newEx.manifestErrors.concat(newEx.runtimeErrors);
        extensions.ExtensionErrorOverlay.getInstance().setErrorsAndShowOverlay(
            errors, extensionId, newEx.name);
      }.bind(this));

      wrapper.setupColumn('suspiciousLearnMore',
                          '.suspicious-install-message .learn-more-link');

      // The path, if provided by unpacked extension.
      wrapper.setupColumn('loadPath', '.load-path a:first-of-type', 'click',
                          function(e) {
        chrome.developerPrivate.showPath(extension.id);
        e.preventDefault();
      });

      // The 'Show Browser Action' button.
      wrapper.setupColumn('showButton', '.show-button', 'click', function(e) {
        chrome.developerPrivate.updateExtensionConfiguration({
          extensionId: extension.id,
          showActionButton: true
        });
      });

      // The 'allow in incognito' checkbox.
      wrapper.setupColumn('incognito', '.incognito-control input', 'change',
                          function(e) {
        var butterBar = wrapper.querySelector('.butter-bar');
        var checked = e.target.checked;
        butterBar.hidden = !checked ||
            extension.type == ExtensionType.HOSTED_APP;
        chrome.developerPrivate.updateExtensionConfiguration({
          extensionId: extension.id,
          incognitoAccess: e.target.checked
        });
      }.bind(this));

      // The 'collect errors' checkbox. This should only be visible if the
      // error console is enabled - we can detect this by the existence of the
      // |errorCollectionEnabled| property.
      wrapper.setupColumn('collectErrors', '.error-collection-control input',
          'change', function(e) {
        chrome.developerPrivate.updateExtensionConfiguration({
          extensionId: extension.id,
          errorCollection: e.target.checked
        });
      });

      // The 'allow on all urls' checkbox. This should only be visible if
      // active script restrictions are enabled. If they are not enabled, no
      // extensions should want all urls.
      wrapper.setupColumn('allUrls', '.all-urls-control input', 'click',
                          function(e) {
        chrome.developerPrivate.updateExtensionConfiguration({
          extensionId: extension.id,
          runOnAllUrls: e.target.checked
        });
      });

      // The 'allow file:// access' checkbox.
      wrapper.setupColumn('localUrls', '.file-access-control input', 'click',
                          function(e) {
        chrome.developerPrivate.updateExtensionConfiguration({
          extensionId: extension.id,
          fileAccess: e.target.checked
        });
      });

      // The 'Reload' terminated link.
      wrapper.setupColumn('terminatedReload', '.terminated-reload-link',
                          'click', function(e) {
        chrome.developerPrivate.reload(extension.id, {failQuietly: true});
      });

      // The 'Repair' corrupted link.
      wrapper.setupColumn('repair', '.corrupted-repair-button', 'click',
                          function(e) {
        chrome.developerPrivate.repairExtension(extension.id);
      });

      // The 'Enabled' checkbox.
      wrapper.setupColumn('enabled', '.enable-checkbox input', 'click',
                          function(e) {
        var checked = e.target.checked;
        // TODO(devlin): What should we do if this fails? Ideally we want to
        // show some kind of error or feedback to the user if this fails.
        chrome.management.setEnabled(extension.id, checked);

        // This may seem counter-intuitive (to not set/clear the checkmark)
        // but this page will be updated asynchronously if the extension
        // becomes enabled/disabled. It also might not become enabled or
        // disabled, because the user might e.g. get prompted when enabling
        // and choose not to.
        e.preventDefault();
      });

      // 'Remove' button.
      var trash = cloneTemplate('trash');
      trash.title = loadTimeData.getString('extensionUninstall');

      wrapper.querySelector('.enable-controls').appendChild(trash);

      wrapper.setupColumn('remove-enterprise', '.trash', 'click', function(e) {
        trash.classList.add('open');
        trash.classList.toggle('mouse-clicked', e.detail > 0);
        if (this.uninstallIsShowing_)
          return;
        this.uninstallIsShowing_ = true;
        chrome.management.uninstall(extension.id,
                                    {showConfirmDialog: true},
                                    function() {
          // TODO(devlin): What should we do if the uninstall fails?
          this.uninstallIsShowing_ = false;

          if (trash.classList.contains('mouse-clicked'))
            trash.blur();

          if (chrome.runtime.lastError) {
            // The uninstall failed (e.g. a cancel). Allow the trash to close.
            trash.classList.remove('open');
          } else {
            // Leave the trash open if the uninstall succeded. Otherwise it can
            // half-close right before it's removed when the DOM is modified.
          }
        }.bind(this));
      }.bind(this));

      // Maintain the order that nodes should be in when creating as well as
      // when adding only one new wrapper.
      this.insertBefore(wrapper, nextWrapper);
      this.updateWrapper_(extension, wrapper);

      var nextRow = this.focusGrid_.getRowForRoot(nextWrapper);  // May be null.
      this.focusGrid_.addRowBefore(wrapper.getFocusRow(), nextRow);
    },

    /**
     * Updates an HTML element for the extension metadata given in |extension|.
     * @param {!chrome.developerPrivate.ExtensionInfo} extension A dictionary of
     *     extension metadata.
     * @param {!Element} wrapper The extension wrapper element to update.
     * @private
     */
    updateWrapper_: function(extension, wrapper) {
      var isActive =
          extension.state == chrome.developerPrivate.ExtensionState.ENABLED;
      wrapper.classList.toggle('inactive-extension', !isActive);
      wrapper.classList.remove('controlled', 'may-not-remove');

      if (extension.controlledInfo) {
        wrapper.classList.add('controlled');
      } else if (!extension.userMayModify ||
                 extension.mustRemainInstalled ||
                 extension.dependentExtensions.length > 0) {
        wrapper.classList.add('may-not-remove');
      }

      var item = wrapper.querySelector('.extension-list-item');
      item.style.backgroundImage = 'url(' + extension.iconUrl + ')';

      this.setText_(wrapper, '.extension-title', extension.name);
      this.setText_(wrapper, '.extension-version', extension.version);
      this.setText_(wrapper, '.location-text', extension.locationText || '');
      this.setText_(wrapper, '.blacklist-text', extension.blacklistText || '');
      this.setText_(wrapper, '.extension-description', extension.description);

      // The 'Show Browser Action' button.
      this.updateVisibility_(wrapper, '.show-button',
                             isActive && extension.actionButtonHidden);

      // The 'allow in incognito' checkbox.
      this.updateVisibility_(wrapper, '.incognito-control',
                             isActive && this.incognitoAvailable_,
                             function(item) {
        var incognito = item.querySelector('input');
        incognito.disabled = !extension.incognitoAccess.isEnabled;
        incognito.checked = extension.incognitoAccess.isActive;
      });
      var showButterBar = isActive &&
            extension.incognitoAccess.isActive &&
            extension.type != ExtensionType.HOSTED_APP;
      // The 'allow in incognito' butter bar.
      this.updateVisibility_(wrapper, '.butter-bar', showButterBar);

      // The 'collect errors' checkbox. This should only be visible if the
      // error console is enabled - we can detect this by the existence of the
      // |errorCollectionEnabled| property.
      this.updateVisibility_(
          wrapper, '.error-collection-control',
          isActive && extension.errorCollection.isEnabled,
          function(item) {
        item.querySelector('input').checked =
            extension.errorCollection.isActive;
      });

      // The 'allow on all urls' checkbox. This should only be visible if
      // active script restrictions are enabled. If they are not enabled, no
      // extensions should want all urls.
      this.updateVisibility_(
          wrapper, '.all-urls-control',
          isActive && extension.runOnAllUrls.isEnabled,
          function(item) {
        item.querySelector('input').checked = extension.runOnAllUrls.isActive;
      });

      // The 'allow file:// access' checkbox.
      this.updateVisibility_(wrapper, '.file-access-control',
                             isActive && extension.fileAccess.isEnabled,
                             function(item) {
        item.querySelector('input').checked = extension.fileAccess.isActive;
      });

      // The 'Options' button or link, depending on its behaviour.
      var optionsEnabled = isActive && !!extension.optionsPage;
      this.updateVisibility_(wrapper, '.options-link', optionsEnabled &&
                             extension.optionsPage.openInTab);
      this.updateVisibility_(wrapper, '.options-button', optionsEnabled &&
                             !extension.optionsPage.openInTab);

      // The 'View in Web Store/View Web Site' link.
      var siteLinkEnabled = !!extension.homePage.url &&
                            !this.enableAppInfoDialog_;
      this.updateVisibility_(wrapper, '.site-link', siteLinkEnabled,
                             function(item) {
        item.href = extension.homePage.url;
        item.textContent = loadTimeData.getString(
            extension.homePage.specified ? 'extensionSettingsVisitWebsite' :
                                           'extensionSettingsVisitWebStore');
      });

      var isUnpacked =
          extension.location == chrome.developerPrivate.Location.UNPACKED;
      // The 'Reload' link.
      this.updateVisibility_(wrapper, '.reload-link', isActive && isUnpacked);

      // The 'Launch' link.
      this.updateVisibility_(
          wrapper, '.launch-link',
          isUnpacked && extension.type == ExtensionType.PLATFORM_APP &&
              isActive);

      // The 'Errors' link.
      var hasErrors = extension.runtimeErrors.length > 0 ||
          extension.manifestErrors.length > 0;
      this.updateVisibility_(wrapper, '.errors-link', hasErrors,
                             function(item) {
        var Level = chrome.developerPrivate.ErrorLevel;

        var map = {};
        map[Level.LOG] = {weight: 0, name: 'extension-error-info-icon'};
        map[Level.WARN] = {weight: 1, name: 'extension-error-warning-icon'};
        map[Level.ERROR] = {weight: 2, name: 'extension-error-fatal-icon'};

        // Find the highest severity of all the errors; manifest errors all have
        // a 'warning' level severity.
        var highestSeverity = extension.runtimeErrors.reduce(
            function(prev, error) {
          return map[error.severity].weight > map[prev].weight ?
              error.severity : prev;
        }, extension.manifestErrors.length ? Level.WARN : Level.LOG);

        // Adjust the class on the icon.
        var icon = item.querySelector('.extension-error-icon');
        // TODO(hcarmona): Populate alt text with a proper description since
        // this icon conveys the severity of the error. (info, warning, fatal).
        icon.alt = '';
        icon.className = 'extension-error-icon';  // Remove other classes.
        icon.classList.add(map[highestSeverity].name);
      });

      // The 'Reload' terminated link.
      var isTerminated =
          extension.state == chrome.developerPrivate.ExtensionState.TERMINATED;
      this.updateVisibility_(wrapper, '.terminated-reload-link', isTerminated);

      // The 'Repair' corrupted link.
      var canRepair = !isTerminated &&
                      extension.disableReasons.corruptInstall &&
                      extension.location ==
                          chrome.developerPrivate.Location.FROM_STORE;
      this.updateVisibility_(wrapper, '.corrupted-repair-button', canRepair);

      // The 'Enabled' checkbox.
      var isOK = !isTerminated && !canRepair;
      this.updateVisibility_(wrapper, '.enable-checkbox', isOK, function(item) {
        var enableCheckboxDisabled =
            !extension.userMayModify ||
            extension.disableReasons.suspiciousInstall ||
            extension.disableReasons.corruptInstall ||
            extension.disableReasons.updateRequired ||
            extension.dependentExtensions.length > 0 ||
            extension.state ==
                chrome.developerPrivate.ExtensionState.BLACKLISTED;
        item.querySelector('input').disabled = enableCheckboxDisabled;
        item.querySelector('input').checked = isActive;
      });

      // Indicator for extensions controlled by policy.
      var controlNode = wrapper.querySelector('.enable-controls');
      var indicator =
          controlNode.querySelector('.controlled-extension-indicator');
      var needsIndicator = isOK && extension.controlledInfo;

      if (needsIndicator && !indicator) {
        indicator = new cr.ui.ControlledIndicator();
        indicator.classList.add('controlled-extension-indicator');
        var ControllerType = chrome.developerPrivate.ControllerType;
        var controlledByStr = '';
        switch (extension.controlledInfo.type) {
          case ControllerType.POLICY:
            controlledByStr = 'policy';
            break;
          case ControllerType.CHILD_CUSTODIAN:
            controlledByStr = 'child-custodian';
            break;
          case ControllerType.SUPERVISED_USER_CUSTODIAN:
            controlledByStr = 'supervised-user-custodian';
            break;
        }
        indicator.setAttribute('controlled-by', controlledByStr);
        var text = extension.controlledInfo.text;
        indicator.setAttribute('text' + controlledByStr, text);
        indicator.image.setAttribute('aria-label', text);
        controlNode.appendChild(indicator);
        wrapper.setupColumn('remove-enterprise', '[controlled-by] div');
      } else if (!needsIndicator && indicator) {
        controlNode.removeChild(indicator);
      }

      // Developer mode ////////////////////////////////////////////////////////

      // First we have the id.
      var idLabel = wrapper.querySelector('.extension-id');
      idLabel.textContent = ' ' + extension.id;

      // Then the path, if provided by unpacked extension.
      this.updateVisibility_(wrapper, '.load-path', isUnpacked,
                             function(item) {
        item.querySelector('a:first-of-type').textContent =
            ' ' + extension.prettifiedPath;
      });

      // Then the 'managed, cannot uninstall/disable' message.
      // We would like to hide managed installed message since this
      // extension is disabled.
      var isRequired =
          !extension.userMayModify || extension.mustRemainInstalled;
      this.updateVisibility_(wrapper, '.managed-message', isRequired &&
                             !extension.disableReasons.updateRequired);

      // Then the 'This isn't from the webstore, looks suspicious' message.
      var isSuspicious = extension.disableReasons.suspiciousInstall;
      this.updateVisibility_(wrapper, '.suspicious-install-message',
                             !isRequired && isSuspicious);

      // Then the 'This is a corrupt extension' message.
      this.updateVisibility_(wrapper, '.corrupt-install-message', !isRequired &&
                             extension.disableReasons.corruptInstall);

      // Then the 'An update required by enterprise policy' message. Note that
      // a force-installed extension might be disabled due to being outdated
      // as well.
      this.updateVisibility_(wrapper, '.update-required-message',
                             extension.disableReasons.updateRequired);

      // The 'following extensions depend on this extension' list.
      var hasDependents = extension.dependentExtensions.length > 0;
      wrapper.classList.toggle('developer-extras', hasDependents);
      this.updateVisibility_(wrapper, '.dependent-extensions-message',
                             hasDependents, function(item) {
        var dependentList = item.querySelector('ul');
        dependentList.textContent = '';
        extension.dependentExtensions.forEach(function(dependentExtension) {
          var depNode = cloneTemplate('dependent-list-item');
          depNode.querySelector('.dep-extension-title').textContent =
              dependentExtension.name;
          depNode.querySelector('.dep-extension-id').textContent =
              dependentExtension.id;
          dependentList.appendChild(depNode);
        }, this);
      }.bind(this));

      // The active views.
      this.updateVisibility_(wrapper, '.active-views',
                             extension.views.length > 0, function(item) {
        var link = item.querySelector('a');

        // Link needs to be an only child before the list is updated.
        while (link.nextElementSibling)
          item.removeChild(link.nextElementSibling);

        // Link needs to be cleaned up if it was used before.
        link.textContent = '';
        if (link.clickHandler)
          link.removeEventListener('click', link.clickHandler);

        extension.views.forEach(function(view, i) {
          if (view.type == chrome.developerPrivate.ViewType.EXTENSION_DIALOG ||
              view.type == chrome.developerPrivate.ViewType.EXTENSION_POPUP) {
            return;
          }
          var displayName;
          if (view.url.indexOf('chrome-extension://') == 0) {
            var pathOffset = 'chrome-extension://'.length + 32 + 1;
            displayName = view.url.substring(pathOffset);
            if (displayName == '_generated_background_page.html')
              displayName = loadTimeData.getString('backgroundPage');
          } else {
            displayName = view.url;
          }
          var label = displayName +
              (view.incognito ?
                  ' ' + loadTimeData.getString('viewIncognito') : '') +
              (view.renderProcessId == -1 ?
                  ' ' + loadTimeData.getString('viewInactive') : '') +
              (view.isIframe ?
                  ' ' + loadTimeData.getString('viewIframe') : '');
          link.textContent = label;
          link.clickHandler = function(e) {
            chrome.developerPrivate.openDevTools({
              extensionId: extension.id,
              renderProcessId: view.renderProcessId,
              renderViewId: view.renderViewId,
              incognito: view.incognito
            });
          };
          link.addEventListener('click', link.clickHandler);

          if (i < extension.views.length - 1) {
            link = link.cloneNode(true);
            item.appendChild(link);
          }

          wrapper.setupColumn('activeView', '.active-views a:last-of-type');
        });
      });

      // The extension warnings (describing runtime issues).
      this.updateVisibility_(wrapper, '.extension-warnings',
                             extension.runtimeWarnings.length > 0,
                             function(item) {
        var warningList = item.querySelector('ul');
        warningList.textContent = '';
        extension.runtimeWarnings.forEach(function(warning) {
          var li = document.createElement('li');
          warningList.appendChild(li).innerText = warning;
        });
      });

      // Install warnings.
      this.updateVisibility_(wrapper, '.install-warnings',
                             extension.installWarnings.length > 0,
                             function(item) {
        var installWarningList = item.querySelector('ul');
        installWarningList.textContent = '';
        if (extension.installWarnings) {
          extension.installWarnings.forEach(function(warning) {
            var li = document.createElement('li');
            li.innerText = warning;
            installWarningList.appendChild(li);
          });
        }
      });

      if (location.hash.substr(1) == extension.id) {
        // Scroll beneath the fixed header so that the extension is not
        // obscured.
        var topScroll = wrapper.offsetTop - $('page-header').offsetHeight;
        var pad = parseInt(window.getComputedStyle(wrapper).marginTop, 10);
        if (!isNaN(pad))
          topScroll -= pad / 2;
        setScrollTopForDocument(document, topScroll);
      }
    },

    /**
     * Updates an element's textContent.
     * @param {Node} node Ancestor of the element specified by |query|.
     * @param {string} query A query to select an element in |node|.
     * @param {string} textContent
     * @private
     */
    setText_: function(node, query, textContent) {
      node.querySelector(query).textContent = textContent;
    },

    /**
     * Updates an element's visibility and calls |shownCallback| if it is
     * visible.
     * @param {Node} node Ancestor of the element specified by |query|.
     * @param {string} query A query to select an element in |node|.
     * @param {boolean} visible Whether the element should be visible or not.
     * @param {function(Element)=} opt_shownCallback Callback if the element is
     *     visible. The element passed in will be the element specified by
     *     |query|.
     * @private
     */
    updateVisibility_: function(node, query, visible, opt_shownCallback) {
      var element = assertInstanceof(node.querySelector(query), Element);
      element.hidden = !visible;
      if (visible && opt_shownCallback)
        opt_shownCallback(element);
    },

    /**
     * Opens the extension options overlay for the extension with the given id.
     * @param {string} extensionId The id of extension whose options page should
     *     be displayed.
     * @param {boolean} scroll Whether the page should scroll to the extension
     * @private
     */
    showEmbeddedExtensionOptions_: function(extensionId, scroll) {
      if (this.optionsShown_)
        return;

      // Get the extension from the given id.
      var extension = this.extensions_.filter(function(extension) {
        return extension.state ==
                   chrome.developerPrivate.ExtensionState.ENABLED &&
               extension.id == extensionId;
      })[0];

      if (!extension)
        return;

      if (scroll)
        this.scrollToWrapper_(extensionId);

      // Add the options query string. Corner case: the 'options' query string
      // will clobber the 'id' query string if the options link is clicked when
      // 'id' is in the URL, or if both query strings are in the URL.
      uber.replaceState({}, '?options=' + extensionId);

      var overlay = extensions.ExtensionOptionsOverlay.getInstance();
      var shownCallback = function() {
        // This overlay doesn't get focused automatically as <extensionoptions>
        // is created after the overlay is shown.
        if (cr.ui.FocusOutlineManager.forDocument(document).visible)
          overlay.setInitialFocus();
      };
      overlay.setExtensionAndShow(extensionId, extension.name,
                                  extension.iconUrl, shownCallback);
      this.optionsShown_ = true;

      var self = this;
      $('overlay').addEventListener('cancelOverlay', function f() {
        self.optionsShown_ = false;
        $('overlay').removeEventListener('cancelOverlay', f);

        // Remove the options query string.
        uber.replaceState({}, '');
      });

      // TODO(dbeam): why do we need to focus <extensionoptions> before and
      // after its showing animation? Makes very little sense to me.
      overlay.setInitialFocus();
    },

    /**
     * Hides the extension options overlay for the extension with id
     * |extensionId|. If there is an overlay showing for a different extension,
     * nothing happens.
     * @param {string} extensionId ID of the extension to hide.
     * @private
     */
    hideEmbeddedExtensionOptions_: function(extensionId) {
      if (!this.optionsShown_)
        return;

      var overlay = extensions.ExtensionOptionsOverlay.getInstance();
      if (overlay.getExtensionId() == extensionId)
        overlay.close();
    },

    /**
     * Updates or creates a wrapper for |extension|.
     * @param {!chrome.developerPrivate.ExtensionInfo} extension The information
     *     about the extension to update.
     * @private
     */
    updateOrCreateWrapper_: function(extension) {
      var currIndex = this.getIndexOfExtension_(extension.id);
      if (currIndex != -1) {
        // If there is a current version of the extension, update it with the
        // new version.
        this.extensions_[currIndex] = extension;
      } else {
        // If the extension isn't found, push it back and sort. Technically, we
        // could optimize by inserting it at the right location, but since this
        // only happens on extension install, it's not worth it.
        this.extensions_.push(extension);
        this.extensions_.sort(compareExtensions);
      }

      var wrapper = $(extension.id);
      if (wrapper) {
        this.updateWrapper_(extension, wrapper);
      } else {
        var nextExt = this.extensions_[this.extensions_.indexOf(extension) + 1];
        this.createWrapper_(extension, nextExt ? $(nextExt.id) : null);
      }
    }
  };

  return {
    ExtensionList: ExtensionList,
    ExtensionListDelegate: ExtensionListDelegate
  };
});
