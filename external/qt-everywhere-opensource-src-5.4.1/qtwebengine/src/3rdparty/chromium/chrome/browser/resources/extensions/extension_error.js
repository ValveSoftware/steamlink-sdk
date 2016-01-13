// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_error_overlay.js"></include>

cr.define('extensions', function() {
  'use strict';

  /**
   * Clone a template within the extension error template collection.
   * @param {string} templateName The class name of the template to clone.
   * @return {HTMLElement} The clone of the template.
   */
  function cloneTemplate(templateName) {
    return $('template-collection-extension-error').
        querySelector('.' + templateName).cloneNode(true);
  }

  /**
   * Checks that an Extension ID follows the proper format (i.e., is 32
   * characters long, is lowercase, and contains letters in the range [a, p]).
   * @param {string} id The Extension ID to test.
   * @return {boolean} Whether or not the ID is valid.
   */
  function idIsValid(id) {
    return /^[a-p]{32}$/.test(id);
  }

  /**
   * Creates a new ExtensionError HTMLElement; this is used to show a
   * notification to the user when an error is caused by an extension.
   * @param {Object} error The error the element should represent.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionError(error) {
    var div = cloneTemplate('extension-error-metadata');
    div.__proto__ = ExtensionError.prototype;
    div.decorate(error);
    return div;
  }

  ExtensionError.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function(error) {
      // Add an additional class for the severity level.
      if (error.level == 0)
        this.classList.add('extension-error-severity-info');
      else if (error.level == 1)
        this.classList.add('extension-error-severity-warning');
      else
        this.classList.add('extension-error-severity-fatal');

      var iconNode = document.createElement('img');
      iconNode.className = 'extension-error-icon';
      this.insertBefore(iconNode, this.firstChild);

      var messageSpan = this.querySelector('.extension-error-message');
      messageSpan.textContent = error.message;
      messageSpan.title = error.message;

      var extensionUrl = 'chrome-extension://' + error.extensionId + '/';
      var viewDetailsLink = this.querySelector('.extension-error-view-details');

      // If we cannot open the file source and there are no external frames in
      // the stack, then there are no details to display.
      if (!extensions.ExtensionErrorOverlay.canShowOverlayForError(
              error, extensionUrl)) {
        viewDetailsLink.hidden = true;
      } else {
        var stringId = extensionUrl.toLowerCase() == 'manifest.json' ?
            'extensionErrorViewManifest' : 'extensionErrorViewDetails';
        viewDetailsLink.textContent = loadTimeData.getString(stringId);

        viewDetailsLink.addEventListener('click', function(e) {
          extensions.ExtensionErrorOverlay.getInstance().setErrorAndShowOverlay(
              error, extensionUrl);
        });
      }
    },
  };

  /**
   * A variable length list of runtime or manifest errors for a given extension.
   * @param {Array.<Object>} errors The list of extension errors with which
   *     to populate the list.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionErrorList(errors) {
    var div = cloneTemplate('extension-error-list');
    div.__proto__ = ExtensionErrorList.prototype;
    div.errors_ = errors;
    div.decorate();
    return div;
  }

  ExtensionErrorList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * @private
     * @const
     * @type {number}
     */
    MAX_ERRORS_TO_SHOW_: 3,

    /** @override */
    decorate: function() {
      this.contents_ = this.querySelector('.extension-error-list-contents');
      this.errors_.forEach(function(error) {
        if (idIsValid(error.extensionId)) {
          this.contents_.appendChild(document.createElement('li')).appendChild(
              new ExtensionError(error));
        }
      }, this);

      if (this.contents_.children.length > this.MAX_ERRORS_TO_SHOW_)
        this.initShowMoreButton_();
    },

    /**
     * Initialize the "Show More" button for the error list. If there are more
     * than |MAX_ERRORS_TO_SHOW_| errors in the list.
     * @private
     */
    initShowMoreButton_: function() {
      var button = this.querySelector('.extension-error-list-show-more button');
      button.hidden = false;
      button.isShowingAll = false;
      var listContents = this.querySelector('.extension-error-list-contents');
      listContents.addEventListener('webkitTransitionEnd', function(e) {
        if (listContents.classList.contains('active'))
          listContents.classList.add('scrollable');
      });
      button.addEventListener('click', function(e) {
        // Disable scrolling while transitioning. If the element is active,
        // scrolling is enabled when the transition ends.
        listContents.classList.toggle('active');
        listContents.classList.remove('scrollable');
        var message = button.isShowingAll ? 'extensionErrorsShowMore' :
                                            'extensionErrorsShowFewer';
        button.textContent = loadTimeData.getString(message);
        button.isShowingAll = !button.isShowingAll;
      }.bind(this));
    }
  };

  return {
    ExtensionErrorList: ExtensionErrorList
  };
});
