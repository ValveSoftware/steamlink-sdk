// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Clone a template within the extension error template collection.
   * @param {string} templateName The class name of the template to clone.
   * @return {HTMLElement} The clone of the template.
   */
  function cloneTemplate(templateName) {
    return /** @type {HTMLElement} */($('template-collection-extension-error').
        querySelector('.' + templateName).cloneNode(true));
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
   * @param {!Array<(ManifestError|RuntimeError)>} errors
   * @param {number} id
   * @return {number} The index of the error with |id|, or -1 if not found.
   */
  function findErrorById(errors, id) {
    for (var i = 0; i < errors.length; ++i) {
      if (errors[i].id == id)
        return i;
    }
    return -1;
  }

  /**
   * Creates a new ExtensionError HTMLElement; this is used to show a
   * notification to the user when an error is caused by an extension.
   * @param {(RuntimeError|ManifestError)} error The error the element should
   *     represent.
   * @constructor
   * @extends {HTMLElement}
   */
  function ExtensionError(error) {
    var div = cloneTemplate('extension-error-metadata');
    div.__proto__ = ExtensionError.prototype;
    div.decorate(error);
    return div;
  }

  ExtensionError.prototype = {
    __proto__: HTMLElement.prototype,

    /**
     * @param {(RuntimeError|ManifestError)} error The error the element should
     *     represent.
     * @private
     */
    decorate: function(error) {
      /**
       * The backing error.
       * @type {(ManifestError|RuntimeError)}
       */
      this.error = error;
      var iconAltTextKey = 'extensionLogLevelWarn';

      // Add an additional class for the severity level.
      if (error.type == chrome.developerPrivate.ErrorType.RUNTIME) {
        switch (error.severity) {
          case chrome.developerPrivate.ErrorLevel.LOG:
            this.classList.add('extension-error-severity-info');
            iconAltTextKey = 'extensionLogLevelInfo';
            break;
          case chrome.developerPrivate.ErrorLevel.WARN:
            this.classList.add('extension-error-severity-warning');
            break;
          case chrome.developerPrivate.ErrorLevel.ERROR:
            this.classList.add('extension-error-severity-fatal');
            iconAltTextKey = 'extensionLogLevelError';
            break;
          default:
            assertNotReached();
        }
      } else {
        // We classify manifest errors as "warnings".
        this.classList.add('extension-error-severity-warning');
      }

      var iconNode = document.createElement('img');
      iconNode.className = 'extension-error-icon';
      iconNode.alt = loadTimeData.getString(iconAltTextKey);
      this.insertBefore(iconNode, this.firstChild);

      var messageSpan = this.querySelector('.extension-error-message');
      messageSpan.textContent = error.message;

      var deleteButton = this.querySelector('.error-delete-button');
      deleteButton.addEventListener('click', function(e) {
        this.dispatchEvent(
            new CustomEvent('deleteExtensionError',
                            {bubbles: true, detail: this.error}));
      }.bind(this));

      this.addEventListener('click', function(e) {
        if (e.target != deleteButton)
          this.requestActive_();
      }.bind(this));

      this.addEventListener('keydown', function(e) {
        if (e.key == 'Enter' && e.target != deleteButton)
          this.requestActive_();
      });
    },

    /**
     * Bubble up an event to request to become active.
     * @private
     */
    requestActive_: function() {
      this.dispatchEvent(
          new CustomEvent('highlightExtensionError',
                          {bubbles: true, detail: this.error}));
    },
  };

  /**
   * A variable length list of runtime or manifest errors for a given extension.
   * @param {Array<(RuntimeError|ManifestError)>} errors The list of extension
   *     errors with which to populate the list.
   * @param {string} extensionId The id of the extension.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionErrorList(errors, extensionId) {
    var div = cloneTemplate('extension-error-list');
    div.__proto__ = ExtensionErrorList.prototype;
    div.extensionId_ = extensionId;
    div.decorate(errors);
    return div;
  }

  /**
   * @param {!Element} root
   * @param {?Element} boundary
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  ExtensionErrorList.FocusRow = function(root, boundary) {
    cr.ui.FocusRow.call(this, root, boundary);

    this.addItem('message', '.extension-error-message');
    this.addItem('delete', '.error-delete-button');
  };

  ExtensionErrorList.FocusRow.prototype = {
    __proto__: cr.ui.FocusRow.prototype,
  };

  ExtensionErrorList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initializes the extension error list.
     * @param {Array<(RuntimeError|ManifestError)>} errors The list of errors.
     */
    decorate: function(errors) {
      /** @private {!Array<(ManifestError|RuntimeError)>} */
      this.errors_ = [];

      /** @private {!cr.ui.FocusGrid} */
      this.focusGrid_ = new cr.ui.FocusGrid();

      /** @private {Element} */
      this.listContents_ = this.querySelector('.extension-error-list-contents');

      errors.forEach(this.addError_, this);

      this.focusGrid_.ensureRowActive();

      this.addEventListener('highlightExtensionError', function(e) {
        this.setActiveErrorNode_(e.target);
      });
      this.addEventListener('deleteExtensionError', function(e) {
        this.removeError_(e.detail);
      });

      this.querySelector('#extension-error-list-clear').addEventListener(
          'click', function(e) {
        this.clear(true);
      }.bind(this));

      /**
       * The callback for the extension changed event.
       * @private {function(chrome.developerPrivate.EventData):void}
       */
      this.onItemStateChangedListener_ = function(data) {
        var type = chrome.developerPrivate.EventType;
        if ((data.event_type == type.ERRORS_REMOVED ||
             data.event_type == type.ERROR_ADDED) &&
            data.extensionInfo.id == this.extensionId_) {
          var newErrors = data.extensionInfo.runtimeErrors.concat(
              data.extensionInfo.manifestErrors);
          this.updateErrors_(newErrors);
        }
      }.bind(this);

      chrome.developerPrivate.onItemStateChanged.addListener(
          this.onItemStateChangedListener_);

      /**
       * The active error element in the list.
       * @private {?}
       */
      this.activeError_ = null;

      this.setActiveError(0);
    },

    /**
     * Adds an error to the list.
     * @param {(RuntimeError|ManifestError)} error The error to add.
     * @private
     */
    addError_: function(error) {
      this.querySelector('#no-errors-span').hidden = true;
      this.errors_.push(error);

      var extensionError = new ExtensionError(error);
      this.listContents_.appendChild(extensionError);

      this.focusGrid_.addRow(
          new ExtensionErrorList.FocusRow(extensionError, this.listContents_));
    },

    /**
     * Removes an error from the list.
     * @param {(RuntimeError|ManifestError)} error The error to remove.
     * @private
     */
    removeError_: function(error) {
      var index = 0;
      for (; index < this.errors_.length; ++index) {
        if (this.errors_[index].id == error.id)
          break;
      }
      assert(index != this.errors_.length);
      var errorList = this.querySelector('.extension-error-list-contents');

      var wasActive =
          this.activeError_ && this.activeError_.error.id == error.id;

      this.errors_.splice(index, 1);
      var listElement = errorList.children[index];

      var focusRow = this.focusGrid_.getRowForRoot(listElement);
      this.focusGrid_.removeRow(focusRow);
      this.focusGrid_.ensureRowActive();
      focusRow.destroy();

      // TODO(dbeam): in a world where this UI is actually used, we should
      // probably move the focus before removing |listElement|.
      listElement.parentNode.removeChild(listElement);

      if (wasActive) {
        index = Math.min(index, this.errors_.length - 1);
        this.setActiveError(index);  // Gracefully handles the -1 case.
      }

      chrome.developerPrivate.deleteExtensionErrors({
        extensionId: error.extensionId,
        errorIds: [error.id]
      });

      if (this.errors_.length == 0)
        this.querySelector('#no-errors-span').hidden = false;
    },

    /**
     * Updates the list of errors.
     * @param {!Array<(ManifestError|RuntimeError)>} newErrors The new list of
     *     errors.
     * @private
     */
    updateErrors_: function(newErrors) {
      this.errors_.forEach(function(error) {
        if (findErrorById(newErrors, error.id) == -1)
          this.removeError_(error);
      }, this);
      newErrors.forEach(function(error) {
        var index = findErrorById(this.errors_, error.id);
        if (index == -1)
          this.addError_(error);
        else
          this.errors_[index] = error;  // Update the existing reference.
      }, this);
    },

    /**
     * Called when the list is being removed.
     */
    onRemoved: function() {
      chrome.developerPrivate.onItemStateChanged.removeListener(
          this.onItemStateChangedListener_);
      this.clear(false);
    },

    /**
     * Sets the active error in the list.
     * @param {number} index The index to set to be active.
     */
    setActiveError: function(index) {
      var errorList = this.querySelector('.extension-error-list-contents');
      var item = errorList.children[index];
      this.setActiveErrorNode_(
          item ? item.querySelector('.extension-error-metadata') : null);
      var node = null;
      if (index >= 0 && index < errorList.children.length) {
        node = errorList.children[index].querySelector(
                   '.extension-error-metadata');
      }
      this.setActiveErrorNode_(node);
    },

    /**
     * Clears the list of all errors.
     * @param {boolean} deleteErrors Whether or not the errors should be deleted
     *     on the backend.
     */
    clear: function(deleteErrors) {
      if (this.errors_.length == 0)
        return;

      if (deleteErrors) {
        var ids = this.errors_.map(function(error) { return error.id; });
        chrome.developerPrivate.deleteExtensionErrors({
          extensionId: this.extensionId_,
          errorIds: ids
        });
      }

      this.setActiveErrorNode_(null);
      this.errors_.length = 0;
      var errorList = this.querySelector('.extension-error-list-contents');
      while (errorList.firstChild)
        errorList.removeChild(errorList.firstChild);
    },

    /**
     * Sets the active error in the list.
     * @param {?} node The error to make active.
     * @private
     */
    setActiveErrorNode_: function(node) {
      if (this.activeError_)
        this.activeError_.classList.remove('extension-error-active');

      if (node)
        node.classList.add('extension-error-active');

      this.activeError_ = node;

      this.dispatchEvent(
          new CustomEvent('activeExtensionErrorChanged',
                          {bubbles: true, detail: node ? node.error : null}));
    },
  };

  return {
    ExtensionErrorList: ExtensionErrorList
  };
});
