// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ControlledSettingIndicator =
                    options.ControlledSettingIndicator;

  /**
   * A variant of the {@link ControlledSettingIndicator} that shows the status
   * of the hotword search setting, including a bubble to show setup errors
   * (such as failures to download extension resources).
   * @constructor
   * @extends {options.ControlledSettingIndicator}
   */
  var HotwordSearchSettingIndicator = cr.ui.define('span');

  HotwordSearchSettingIndicator.prototype = {
    __proto__: ControlledSettingIndicator.prototype,

    /**
     * Decorates the base element to show the proper icon.
     * @override
     */
    decorate: function() {
      ControlledSettingIndicator.prototype.decorate.call(this);
      this.hidden = true;
    },

    /**
     * Handle changes to the associated pref by hiding any currently visible
     * bubble.
     * @param {Event} event Pref change event.
     * @override
     */
    handlePrefChange: function(event) {
      PageManager.hideBubble();
    },

    /**
     * Sets the variable tracking the section which becomes disabled if an
     * error exists.
     * @param {HTMLElement} section The section to disable.
     */
    set disabledOnErrorSection(section) {
      // TODO(kcarattini): Instead of this, add a cr.EventTarget and have
      // the element to disable register an event on it.
      this.disabledOnErrorSection_ = section;
    },

    /**
     * Returns the current error.
     * @return {string} The error message to be displayed. May be undefined if
     *     there is no error.
     */
    get errorText() {
      return this.errorText_;
    },

    /**
     * Checks for errors and records them.
     * @param {string} errorMsg The error message to be displayed. May be
     *     undefined if there is no error.
     */
    setError: function(errorMsg) {
      this.setAttribute('controlled-by', 'policy');
      this.errorText_ = errorMsg;
    },

    /**
     * Changes the display to be visible if there are errors and disables
     * the section.
     */
    updateBasedOnError: function() {
      if (this.errorText_)
        this.hidden = false;
      if (this.disabledOnErrorSection_)
        this.disabledOnErrorSection_.disabled = !!this.errorText_;
    },

    /**
     * Toggles showing and hiding the error message bubble. An empty
     * |errorText_| indicates that there is no error message. So the bubble
     * only be shown if |errorText_| has a value.
     * @override
     */
    toggleBubble: function() {
      if (this.showingBubble) {
        PageManager.hideBubble();
        return;
      }

      if (!this.errorText_)
        return;

      var self = this;
      // Create the DOM tree for the bubble content.
      var closeButton = document.createElement('div');
      closeButton.classList.add('close-button');

      var text = document.createElement('p');
      text.innerHTML = this.errorText_;

      var textDiv = document.createElement('div');
      textDiv.appendChild(text);

      var container = document.createElement('div');
      container.appendChild(closeButton);
      container.appendChild(textDiv);

      var content = document.createElement('div');
      content.appendChild(container);

      PageManager.showBubble(content, this.image, this, this.location);
    },
  };

  // Export
  return {
    HotwordSearchSettingIndicator: HotwordSearchSettingIndicator
  };
});
