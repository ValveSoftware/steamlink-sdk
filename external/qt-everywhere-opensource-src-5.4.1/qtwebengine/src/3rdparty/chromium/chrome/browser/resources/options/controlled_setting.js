// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Preferences = options.Preferences;

  /**
   * A controlled setting indicator that can be placed on a setting as an
   * indicator that the value is controlled by some external entity such as
   * policy or an extension.
   * @constructor
   * @extends {HTMLSpanElement}
   */
  var ControlledSettingIndicator = cr.ui.define('span');

  ControlledSettingIndicator.prototype = {
    __proto__: cr.ui.BubbleButton.prototype,

    /**
     * Decorates the base element to show the proper icon.
     */
    decorate: function() {
      cr.ui.BubbleButton.prototype.decorate.call(this);
      this.classList.add('controlled-setting-indicator');

      // If there is a pref, track its controlledBy and recommendedValue
      // properties in order to be able to bring up the correct bubble.
      if (this.pref) {
        Preferences.getInstance().addEventListener(
            this.pref, this.handlePrefChange.bind(this));
        this.resetHandler = this.clearAssociatedPref_;
      }
    },

    /**
     * The given handler will be called when the user clicks on the 'reset to
     * recommended value' link shown in the indicator bubble. The |this| object
     * will be the indicator itself.
     * @param {function()} handler The handler to be called.
     */
    set resetHandler(handler) {
      this.resetHandler_ = handler;
    },

    /**
     * Clears the preference associated with this indicator.
     * @private
     */
    clearAssociatedPref_: function() {
      Preferences.clearPref(this.pref, !this.dialogPref);
    },

    /* Handle changes to the associated pref by hiding any currently visible
     * bubble and updating the controlledBy property.
     * @param {Event} event Pref change event.
     */
    handlePrefChange: function(event) {
      OptionsPage.hideBubble();
      if (event.value.controlledBy) {
        if (!this.value || String(event.value.value) == this.value) {
          this.controlledBy = event.value.controlledBy;
          if (event.value.extension) {
            this.extensionId = event.value.extension.id;
            this.extensionIcon = event.value.extension.icon;
            this.extensionName = event.value.extension.name;
          }
        } else {
          this.controlledBy = null;
        }
      } else if (event.value.recommendedValue != undefined) {
        this.controlledBy =
            !this.value || String(event.value.recommendedValue) == this.value ?
            'hasRecommendation' : null;
      } else {
        this.controlledBy = null;
      }
    },

    /**
     * Open or close a bubble with further information about the pref.
     * @private
     */
    toggleBubble_: function() {
      if (this.showingBubble) {
        OptionsPage.hideBubble();
      } else {
        var self = this;

        // Construct the bubble text.
        if (this.hasAttribute('plural')) {
          var defaultStrings = {
            'policy': loadTimeData.getString('controlledSettingsPolicy'),
            'extension': loadTimeData.getString('controlledSettingsExtension'),
            'extensionWithName': loadTimeData.getString(
                'controlledSettingsExtensionWithName'),
          };
          if (cr.isChromeOS) {
            defaultStrings.shared =
                loadTimeData.getString('controlledSettingsShared');
          }
        } else {
          var defaultStrings = {
            'policy': loadTimeData.getString('controlledSettingPolicy'),
            'extension': loadTimeData.getString('controlledSettingExtension'),
            'extensionWithName': loadTimeData.getString(
                'controlledSettingExtensionWithName'),
            'recommended':
                loadTimeData.getString('controlledSettingRecommended'),
            'hasRecommendation':
                loadTimeData.getString('controlledSettingHasRecommendation'),
          };
          if (cr.isChromeOS) {
            defaultStrings.owner =
                loadTimeData.getString('controlledSettingOwner');
            defaultStrings.shared =
                loadTimeData.getString('controlledSettingShared');
          }
        }

        // No controller, no bubble.
        if (!this.controlledBy || !(this.controlledBy in defaultStrings))
          return;

        var text = defaultStrings[this.controlledBy];
        if (this.controlledBy == 'extension' && this.extensionName)
          text = defaultStrings.extensionWithName;

        // Apply text overrides.
        if (this.hasAttribute('text' + this.controlledBy))
          text = this.getAttribute('text' + this.controlledBy);

        // Create the DOM tree.
        var content = document.createElement('div');
        content.classList.add('controlled-setting-bubble-header');
        content.textContent = text;

        if (this.controlledBy == 'hasRecommendation' && this.resetHandler_ &&
            !this.readOnly) {
          var container = document.createElement('div');
          var action = document.createElement('button');
          action.classList.add('link-button');
          action.classList.add('controlled-setting-bubble-action');
          action.textContent =
              loadTimeData.getString('controlledSettingFollowRecommendation');
          action.addEventListener('click', function(event) {
            self.resetHandler_();
          });
          container.appendChild(action);
          content.appendChild(container);
        } else if (this.controlledBy == 'extension' && this.extensionName) {
          var extensionContainer =
              $('extension-controlled-settings-bubble-template').
                  cloneNode(true);
          // No need for an id anymore, and thus remove to avoid id collision.
          extensionContainer.removeAttribute('id');
          extensionContainer.hidden = false;

          var extensionName = extensionContainer.querySelector(
              '.controlled-setting-bubble-extension-name');
          extensionName.textContent = this.extensionName;
          extensionName.style.backgroundImage =
              'url("' + this.extensionIcon + '")';

          var manageLink = extensionContainer.querySelector(
              '.controlled-setting-bubble-extension-manage-link');
          var extensionId = this.extensionId;
          manageLink.onclick = function() {
            uber.invokeMethodOnWindow(
                window.top, 'showPage', {pageId: 'extensions',
                                         path: '?id=' + extensionId});
          };

          var disableButton = extensionContainer.querySelector(
              '.controlled-setting-bubble-extension-disable-button');
          disableButton.onclick = function() {
            chrome.send('disableExtension', [extensionId]);
          };
          content.appendChild(extensionContainer);
        }

        OptionsPage.showBubble(content, this.image, this, this.location);
      }
    },
  };

  /**
   * The name of the associated preference.
   * @type {string}
   */
  cr.defineProperty(ControlledSettingIndicator, 'pref', cr.PropertyKind.ATTR);

  /**
   * Whether this indicator is part of a dialog. If so, changes made to the
   * associated preference take effect in the settings UI immediately but are
   * only actually committed when the user confirms the dialog. If the user
   * cancels the dialog instead, the changes are rolled back in the settings UI
   * and never committed.
   * @type {boolean}
   */
  cr.defineProperty(ControlledSettingIndicator, 'dialogPref',
                    cr.PropertyKind.BOOL_ATTR);

  /**
   * The value of the associated preference that the indicator represents. If
   * this is not set, the indicator will be visible whenever any value is
   * enforced or recommended. If it is set, the indicator will be visible only
   * when the enforced or recommended value matches the value it represents.
   * This allows multiple indicators to be created for a set of radio buttons,
   * ensuring that only one of them is visible at a time.
   */
  cr.defineProperty(ControlledSettingIndicator, 'value',
                    cr.PropertyKind.ATTR);

  /**
   * The status of the associated preference:
   * - 'policy':            A specific value is enfoced by policy.
   * - 'extension':         A specific value is enforced by an extension.
   * - 'recommended':       A value is recommended by policy. The user could
   *                        override this recommendation but has not done so.
   * - 'hasRecommendation': A value is recommended by policy. The user has
   *                        overridden this recommendation.
   * - 'owner':             A value is controlled by the owner of the device
   *                        (Chrome OS only).
   * - 'shared':            A value belongs to the primary user but can be
   *                        modified (Chrome OS only).
   * - unset:               The value is controlled by the user alone.
   * @type {string}
   */
  cr.defineProperty(ControlledSettingIndicator, 'controlledBy',
                    cr.PropertyKind.ATTR);

  // Export.
  return {
    ControlledSettingIndicator: ControlledSettingIndicator
  };
});
