// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.browser_options', function() {
  /** @const */ var AutocompleteList = cr.ui.AutocompleteList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;

  /**
   * Creates a new startup page list item.
   * @param {Object} pageInfo The page this item represents.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function StartupPageListItem(pageInfo) {
    var el = cr.doc.createElement('div');
    el.pageInfo_ = pageInfo;
    StartupPageListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a startup page list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  StartupPageListItem.decorate = function(el) {
    el.__proto__ = StartupPageListItem.prototype;
    el.decorate();
  };

  StartupPageListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /**
     * Input field for editing the page url.
     * @type {HTMLElement}
     * @private
     */
    urlField_: null,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      var pageInfo = this.pageInfo_;

      if (pageInfo.modelIndex == -1) {
        this.isPlaceholder = true;
        pageInfo.title = loadTimeData.getString('startupAddLabel');
        pageInfo.url = '';
      }

      var titleEl = this.ownerDocument.createElement('div');
      titleEl.className = 'title';
      titleEl.classList.add('favicon-cell');
      titleEl.classList.add('weakrtl');
      titleEl.textContent = pageInfo.title;
      if (!this.isPlaceholder) {
        titleEl.style.backgroundImage = cr.icon.getFavicon(pageInfo.url);
        titleEl.title = pageInfo.tooltip;
      }

      this.contentElement.appendChild(titleEl);

      var urlEl = this.createEditableTextCell(pageInfo.url);
      urlEl.className = 'url';
      urlEl.classList.add('weakrtl');
      this.contentElement.appendChild(urlEl);

      var urlField = /** @type {HTMLElement} */(urlEl.querySelector('input'));
      urlField.className = 'weakrtl';
      urlField.placeholder = loadTimeData.getString('startupPagesPlaceholder');
      this.urlField_ = urlField;

      this.addEventListener('commitedit', this.onEditCommitted_);

      var self = this;
      urlField.addEventListener('focus', function(event) {
        self.parentNode.autocompleteList.attachToInput(urlField);
      });
      urlField.addEventListener('blur', function(event) {
        self.parentNode.autocompleteList.detach();
      });

      if (!this.isPlaceholder)
        titleEl.draggable = true;
    },

    /** @override */
    get currentInputIsValid() {
      return this.urlField_.validity.valid;
    },

    /** @override */
    get hasBeenEdited() {
      return this.urlField_.value != this.pageInfo_.url;
    },

    /**
     * Called when committing an edit; updates the model.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var url = this.urlField_.value;
      if (this.isPlaceholder)
        chrome.send('addStartupPage', [url]);
      else
        chrome.send('editStartupPage', [this.pageInfo_.modelIndex, url]);
    },
  };

  var StartupPageList = cr.ui.define('list');

  StartupPageList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * An autocomplete suggestion list for URL editing.
     * @type {AutocompleteList}
     */
    autocompleteList: null,

    /**
     * The drop position information: "below" or "above".
     */
    dropPos: null,

    /** @override */
    decorate: function() {
      InlineEditableItemList.prototype.decorate.call(this);

      // Listen to drag and drop events.
      this.addEventListener('dragstart', this.handleDragStart_.bind(this));
      this.addEventListener('dragenter', this.handleDragEnter_.bind(this));
      this.addEventListener('dragover', this.handleDragOver_.bind(this));
      this.addEventListener('drop', this.handleDrop_.bind(this));
      this.addEventListener('dragleave', this.handleDragLeave_.bind(this));
      this.addEventListener('dragend', this.handleDragEnd_.bind(this));
    },

    /** @override */
    createItem: function(pageInfo) {
      var item = new StartupPageListItem(pageInfo);
      item.urlField_.disabled = this.disabled;
      return item;
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      chrome.send('removeStartupPages', [index]);
    },

    /**
     * Computes the target item of drop event.
     * @param {Event} e The drop or dragover event.
     * @private
     */
    getTargetFromDropEvent_: function(e) {
      var target = e.target;
      // e.target may be an inner element of the list item
      while (target != null && !(target instanceof StartupPageListItem)) {
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
      // Prevent dragging if the list is disabled.
      if (this.disabled) {
        e.preventDefault();
        return false;
      }

      var target = this.getTargetFromDropEvent_(e);
      // StartupPageListItem should be the only draggable element type in the
      // page but let's make sure.
      if (target instanceof StartupPageListItem) {
        this.draggedItem = target;
        this.draggedItem.editable = false;
        e.dataTransfer.effectAllowed = 'move';
        // We need to put some kind of data in the drag or it will be
        // ignored.  Use the URL in case the user drags to a text field or the
        // desktop.
        e.dataTransfer.setData('text/plain', target.urlField_.value);
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
      // The drop is only successful on another StartupPageListItem.
      if (!(dropTarget instanceof StartupPageListItem) ||
          dropTarget == this.draggedItem || dropTarget.isPlaceholder) {
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

      if (!(dropTarget instanceof StartupPageListItem) ||
          dropTarget.pageInfo_.modelIndex == -1) {
        return;
      }

      this.hideDropMarker_();

      // Insert the selection at the new position.
      var newIndex = this.dataModel.indexOf(dropTarget.pageInfo_);
      if (this.dropPos == 'below')
        newIndex += 1;

      // If there are selected indexes, it was a re-order.
      if (this.selectionModel.selectedIndexes.length > 0) {
        chrome.send('dragDropStartupPage',
                    [newIndex, this.selectionModel.selectedIndexes]);
        return;
      }

      // Otherwise it was potentially a drop of new data (e.g. a bookmark).
      var url = e.dataTransfer.getData('url');
      if (url) {
        e.preventDefault();
        chrome.send('addStartupPage', [url, newIndex]);
      }
    },

    /**
     * Handles the dragleave event.
     * @param {Event} e The dragleave event.
     * @private
     */
    handleDragLeave_: function(e) {
      this.hideDropMarker_();
    },

    /**
     * Handles the dragend event.
     * @param {Event} e The dragend event.
     * @private
     */
    handleDragEnd_: function(e) {
      this.draggedItem.editable = true;
      this.draggedItem.updateEditState();
    },

    /**
     * Shows and positions the marker to indicate the drop target.
     * @param {HTMLElement} target The current target list item of drop.
     * @param {string} pos 'below' or 'above'.
     * @private
     */
    showDropMarker_: function(target, pos) {
      window.clearTimeout(this.hideDropMarkerTimer_);
      var marker = $('startupPagesListDropmarker');
      var rect = target.getBoundingClientRect();
      var markerHeight = 6;
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
        $('startupPagesListDropmarker').style.display = '';
      }, 100);
    },
  };

  return {
    StartupPageList: StartupPageList
  };
});
