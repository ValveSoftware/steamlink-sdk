// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(arv): Now that this is driven by a data model, implement a data model
//            that handles the loading and the events from the bookmark backend.

/**
 * @typedef {{childIds: Array<string>}}
 *
 * @see chrome/common/extensions/api/bookmarks.json
 */
var ReorderInfo;

/**
 * @typedef {{parentId: string,
 *            index: number,
 *            oldParentId: string,
 *            oldIndex: number}}
 *
 * @see chrome/common/extensions/api/bookmarks.json
 */
var MoveInfo;

cr.define('bmm', function() {
  'use strict';

  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var ContextMenuButton = cr.ui.ContextMenuButton;

  /**
   * Basic array data model for use with bookmarks.
   * @param {!Array<!BookmarkTreeNode>} items The bookmark items.
   * @constructor
   * @extends {ArrayDataModel}
   */
  function BookmarksArrayDataModel(items) {
    ArrayDataModel.call(this, items);
  }

  BookmarksArrayDataModel.prototype = {
    __proto__: ArrayDataModel.prototype,

    /**
     * Finds the index of the bookmark with the given ID.
     * @param {string} id The ID of the bookmark node to find.
     * @return {number} The index of the found node or -1 if not found.
     */
    findIndexById: function(id) {
      for (var i = 0; i < this.length; i++) {
        if (this.item(i).id == id)
          return i;
      }
      return -1;
    }
  };

  /**
   * Removes all children and appends a new child.
   * @param {!Node} parent The node to remove all children from.
   * @param {!Node} newChild The new child to append.
   */
  function replaceAllChildren(parent, newChild) {
    var n;
    while ((n = parent.lastChild)) {
      parent.removeChild(n);
    }
    parent.appendChild(newChild);
  }

  /**
   * Creates a new bookmark list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var BookmarkList = cr.ui.define('list');

  BookmarkList.prototype = {
    __proto__: List.prototype,

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.addEventListener('mousedown', this.handleMouseDown_);

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', this.redraw.bind(this));

      // We could add the ContextMenuButton in the BookmarkListItem but it slows
      // down redraws a lot so we do this on mouseovers instead.
      this.addEventListener('mouseover', this.handleMouseOver_.bind(this));

      bmm.list = this;
    },

    /**
     * @param {!BookmarkTreeNode} bookmarkNode
     * @override
     */
    createItem: function(bookmarkNode) {
      return new BookmarkListItem(bookmarkNode);
    },

    /** @private {string} */
    parentId_: '',

    /** @private {number} */
    loadCount_: 0,

    /**
     * Reloads the list from the bookmarks backend.
     */
    reload: function() {
      var parentId = this.parentId;

      var callback = this.handleBookmarkCallback_.bind(this);

      this.loadCount_++;

      if (!parentId)
        callback([]);
      else if (/^q=/.test(parentId))
        chrome.bookmarks.search(parentId.slice(2), callback);
      else
        chrome.bookmarks.getChildren(parentId, callback);
    },

    /**
     * Callback function for loading items.
     * @param {Array<!BookmarkTreeNode>} items The loaded items.
     * @private
     */
    handleBookmarkCallback_: function(items) {
      this.loadCount_--;
      if (this.loadCount_)
        return;

      if (!items) {
        // Failed to load bookmarks. Most likely due to the bookmark being
        // removed.
        cr.dispatchSimpleEvent(this, 'invalidId');
        return;
      }

      this.dataModel = new BookmarksArrayDataModel(items);

      this.fixWidth_();
      cr.dispatchSimpleEvent(this, 'load');

      // Use the same histogram configuration as UMA_HISTOGRAM_COUNTS_1000().
      chrome.metricsPrivate.recordValue({
        'metricName': 'Bookmarks.BookmarksInFolder',
        'type': chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
        'min': 1,
        'max': 1000,
        'buckets': 50
      }, this.dataModel.length);
    },

    /**
     * The bookmark node that the list is currently displaying. If we are
     * currently displaying search this returns null.
     * @type {BookmarkTreeNode}
     */
    get bookmarkNode() {
      if (this.isSearch())
        return null;
      var treeItem = bmm.treeLookup[this.parentId];
      return treeItem && treeItem.bookmarkNode;
    },

    /**
     * @return {boolean} Whether we are currently showing search results.
     */
    isSearch: function() {
      return this.parentId_[0] == 'q';
    },

    /**
     * @return {boolean} Whether we are editing an ephemeral item.
     */
    hasEphemeral: function() {
      var dataModel = this.dataModel;
      for (var i = 0; i < dataModel.array_.length; i++) {
        if (dataModel.array_[i].id == 'new')
          return true;
      }
      return false;
    },

    /**
     * Handles mouseover on the list so that we can add the context menu button
     * lazily.
     * @private
     * @param {!Event} e The mouseover event object.
     */
    handleMouseOver_: function(e) {
      var el = e.target;
      while (el && el.parentNode != this) {
        el = el.parentNode;
      }

      if (el && el.parentNode == this &&
          !el.editing &&
          !(el.lastChild instanceof ContextMenuButton)) {
        el.appendChild(new ContextMenuButton);
      }
    },

    /**
     * Dispatches an urlClicked event which is used to open URLs in new
     * tabs etc.
     * @private
     * @param {string} url The URL that was clicked.
     * @param {!Event} originalEvent The original click event object.
     */
    dispatchUrlClickedEvent_: function(url, originalEvent) {
      var event = new Event('urlClicked', {bubbles: true});
      event.url = url;
      event.originalEvent = originalEvent;
      this.dispatchEvent(event);
    },

    /**
     * Handles mousedown events so that we can prevent the auto scroll as
     * necessary.
     * @private
     * @param {!Event} e The mousedown event object.
     */
    handleMouseDown_: function(e) {
      e = /** @type {!MouseEvent} */(e);
      if (e.button == 1) {
        // WebKit no longer fires click events for middle clicks so we manually
        // listen to mouse up to dispatch a click event.
        this.addEventListener('mouseup', this.handleMiddleMouseUp_);

        // When the user does a middle click we need to prevent the auto scroll
        // in case the user is trying to middle click to open a bookmark in a
        // background tab.
        // We do not do this in case the target is an input since middle click
        // is also paste on Linux and we don't want to break that.
        if (e.target.tagName != 'INPUT')
          e.preventDefault();
      }
    },

    /**
     * WebKit no longer dispatches click events for middle clicks so we need
     * to emulate it.
     * @private
     * @param {!Event} e The mouse up event object.
     */
    handleMiddleMouseUp_: function(e) {
      e = /** @type {!MouseEvent} */(e);
      this.removeEventListener('mouseup', this.handleMiddleMouseUp_);
      if (e.button == 1) {
        var el = e.target;
        while (el.parentNode != this) {
          el = el.parentNode;
        }
        var node = el.bookmarkNode;
        if (node && !bmm.isFolder(node))
          this.dispatchUrlClickedEvent_(node.url, e);
      }
      e.preventDefault();
    },

    // Bookmark model update callbacks
    handleBookmarkChanged: function(id, changeInfo) {
      var dataModel = this.dataModel;
      var index = dataModel.findIndexById(id);
      if (index != -1) {
        var bookmarkNode = this.dataModel.item(index);
        bookmarkNode.title = changeInfo.title;
        if ('url' in changeInfo)
          bookmarkNode.url = changeInfo['url'];

        dataModel.updateIndex(index);
      }
    },

    /**
     * @param {string} id
     * @param {ReorderInfo} reorderInfo
     */
    handleChildrenReordered: function(id, reorderInfo) {
      if (this.parentId == id) {
        // We create a new data model with updated items in the right order.
        var dataModel = this.dataModel;
        var items = {};
        for (var i = this.dataModel.length - 1; i >= 0; i--) {
          var bookmarkNode = dataModel.item(i);
          items[bookmarkNode.id] = bookmarkNode;
        }
        var newArray = [];
        for (var i = 0; i < reorderInfo.childIds.length; i++) {
          newArray[i] = items[reorderInfo.childIds[i]];
          newArray[i].index = i;
        }

        this.dataModel = new BookmarksArrayDataModel(newArray);
      }
    },

    handleCreated: function(id, bookmarkNode) {
      if (this.parentId == bookmarkNode.parentId)
        this.dataModel.splice(bookmarkNode.index, 0, bookmarkNode);
    },

    /**
     * @param {string} id
     * @param {MoveInfo} moveInfo
     */
    handleMoved: function(id, moveInfo) {
      if (moveInfo.parentId == this.parentId ||
          moveInfo.oldParentId == this.parentId) {

        var dataModel = this.dataModel;

        if (moveInfo.oldParentId == moveInfo.parentId) {
          // Reorder within this folder

          this.startBatchUpdates();

          var bookmarkNode = this.dataModel.item(moveInfo.oldIndex);
          this.dataModel.splice(moveInfo.oldIndex, 1);
          this.dataModel.splice(moveInfo.index, 0, bookmarkNode);

          this.endBatchUpdates();
        } else {
          if (moveInfo.oldParentId == this.parentId) {
            // Move out of this folder

            var index = dataModel.findIndexById(id);
            if (index != -1)
              dataModel.splice(index, 1);
          }

          if (moveInfo.parentId == this.parentId) {
            // Move to this folder
            var self = this;
            chrome.bookmarks.get(id, function(bookmarkNodes) {
              var bookmarkNode = bookmarkNodes[0];
              dataModel.splice(bookmarkNode.index, 0, bookmarkNode);
            });
          }
        }
      }
    },

    handleRemoved: function(id, removeInfo) {
      var dataModel = this.dataModel;
      var index = dataModel.findIndexById(id);
      if (index != -1)
        dataModel.splice(index, 1);
    },

    /**
     * Workaround for http://crbug.com/40902
     * @private
     */
    fixWidth_: function() {
      var list = bmm.list;
      if (this.loadCount_ || !list)
        return;

      // The width of the list is wrong after its content has changed.
      // Fortunately the reported offsetWidth is correct so we can detect the
      //incorrect width.
      if (list.offsetWidth != list.parentNode.clientWidth - list.offsetLeft) {
        // Set the width to the correct size. This causes the relayout.
        list.style.width = list.parentNode.clientWidth - list.offsetLeft + 'px';
        // Remove the temporary style.width in a timeout. Once the timer fires
        // the size should not change since we already fixed the width.
        window.setTimeout(function() {
          list.style.width = '';
        }, 0);
      }
    }
  };

  /**
   * The ID of the bookmark folder we are displaying.
   */
  cr.defineProperty(BookmarkList, 'parentId', cr.PropertyKind.JS,
                    function() {
                      this.reload();
                    });

  /**
   * The contextMenu property.
   */
  cr.ui.contextMenuHandler.addContextMenuProperty(BookmarkList);
  /** @type {cr.ui.Menu} */
  BookmarkList.prototype.contextMenu;

  /**
   * Creates a new bookmark list item.
   * @param {!BookmarkTreeNode} bookmarkNode The bookmark node this represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function BookmarkListItem(bookmarkNode) {
    var el = cr.doc.createElement('div');
    el.bookmarkNode = bookmarkNode;
    BookmarkListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a bookmark list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  BookmarkListItem.decorate = function(el) {
    el.__proto__ = BookmarkListItem.prototype;
    el.decorate();
  };

  BookmarkListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var bookmarkNode = this.bookmarkNode;

      this.draggable = true;

      var labelEl = this.ownerDocument.createElement('div');
      labelEl.className = 'label';
      var labelImgWrapper = this.ownerDocument.createElement('div');
      labelImgWrapper.className = 'label-img-wrapper';
      var labelImg = this.ownerDocument.createElement('div');
      var labelText = this.ownerDocument.createElement('div');
      labelText.className = 'label-text';
      labelText.textContent = bookmarkNode.title;

      var urlEl = this.ownerDocument.createElement('div');
      urlEl.className = 'url';

      if (bmm.isFolder(bookmarkNode)) {
        this.className = 'folder';
        // TODO(pkasting): Condense folder icon resources together.
        labelImg.style.content = cr.icon.getImage(
            cr.isMac ?
                'chrome://theme/IDR_BOOKMARK_BAR_FOLDER' :
                'chrome://theme/IDR_FOLDER_CLOSED');
      } else {
        labelImg.style.content = cr.icon.getFavicon(bookmarkNode.url);
        urlEl.textContent = bookmarkNode.url;
      }

      labelImgWrapper.appendChild(labelImg);
      labelEl.appendChild(labelImgWrapper);
      labelEl.appendChild(labelText);
      this.appendChild(labelEl);
      this.appendChild(urlEl);

      // Initially the ContextMenuButton was added here but it slowed down
      // rendering a lot so it is now added using mouseover.
    },

    /**
     * The ID of the bookmark folder we are currently showing or loading.
     * @type {string}
     */
    get bookmarkId() {
      return this.bookmarkNode.id;
    },

    /**
     * Whether the user is currently able to edit the list item.
     * @type {boolean}
     */
    get editing() {
      return this.hasAttribute('editing');
    },
    set editing(editing) {
      var oldEditing = this.editing;
      if (oldEditing == editing)
        return;

      var url = this.bookmarkNode.url;
      var title = this.bookmarkNode.title;
      var isFolder = bmm.isFolder(this.bookmarkNode);
      var listItem = this;
      var labelInput, urlInput;

      // Handles enter and escape which trigger reset and commit respectively.
      function handleKeydown(e) {
        // Make sure that the tree does not handle the key.
        e.stopPropagation();

        // Calling list.focus blurs the input which will stop editing the list
        // item.
        switch (e.key) {
          case 'Escape':  // Esc
            labelInput.value = title;
            if (!isFolder)
              urlInput.value = url;
            // fall through
            cr.dispatchSimpleEvent(listItem, 'canceledit', true);
          case 'Enter':
            if (listItem.parentNode)
              listItem.parentNode.focus();
            break;
          case 'Tab':  // Tab
            // urlInput is the last focusable element in the page.  If we
            // allowed Tab focus navigation and the page loses focus, we
            // couldn't give focus on urlInput programatically. So, we prevent
            // Tab focus navigation.
            if (document.activeElement == urlInput && !e.ctrlKey &&
                !e.metaKey && !e.shiftKey && !getValidURL(urlInput)) {
              e.preventDefault();
              urlInput.blur();
            }
            break;
        }
      }

      function getValidURL(input) {
        var originalValue = input.value;
        if (!originalValue)
          return null;
        if (input.validity.valid)
          return originalValue;
        // Blink does not do URL fix up so we manually test if prepending
        // 'http://' would make the URL valid.
        // https://bugs.webkit.org/show_bug.cgi?id=29235
        input.value = 'http://' + originalValue;
        if (input.validity.valid)
          return input.value;
        // still invalid
        input.value = originalValue;
        return null;
      }

      function handleBlur(e) {
        // When the blur event happens we do not know who is getting focus so we
        // delay this a bit since we want to know if the other input got focus
        // before deciding if we should exit edit mode.
        var doc = e.target.ownerDocument;
        window.setTimeout(function() {
          var activeElement = doc.hasFocus() && doc.activeElement;
          if (activeElement != urlInput && activeElement != labelInput) {
            listItem.editing = false;
          }
        }, 50);
      }

      var doc = this.ownerDocument;
      var labelTextEl = queryRequiredElement('.label-text', this);
      var urlEl = queryRequiredElement('.url', this);
      if (editing) {
        this.setAttribute('editing', '');
        this.draggable = false;

        labelInput = /** @type {HTMLElement} */(doc.createElement('input'));
        labelInput.placeholder =
            loadTimeData.getString('name_input_placeholder');
        replaceAllChildren(labelTextEl, labelInput);
        labelInput.value = title;

        if (!isFolder) {
          urlInput = /** @type {HTMLElement} */(doc.createElement('input'));
          urlInput.type = 'url';
          urlInput.required = true;
          urlInput.placeholder =
              loadTimeData.getString('url_input_placeholder');

          // We also need a name for the input for the CSS to work.
          urlInput.name = '-url-input-' + cr.createUid();
          replaceAllChildren(assert(urlEl), urlInput);
          urlInput.value = url;
        }

        var stopPropagation = function(e) {
          e.stopPropagation();
        };

        var eventsToStop =
            ['mousedown', 'mouseup', 'contextmenu', 'dblclick', 'paste'];
        eventsToStop.forEach(function(type) {
          labelInput.addEventListener(type, stopPropagation);
        });
        labelInput.addEventListener('keydown', handleKeydown);
        labelInput.addEventListener('blur', handleBlur);
        cr.ui.limitInputWidth(labelInput, this, 100, 0.5);
        labelInput.focus();
        labelInput.select();

        if (!isFolder) {
          eventsToStop.forEach(function(type) {
            urlInput.addEventListener(type, stopPropagation);
          });
          urlInput.addEventListener('keydown', handleKeydown);
          urlInput.addEventListener('blur', handleBlur);
          cr.ui.limitInputWidth(urlInput, this, 200, 0.5);
        }

      } else {
        // Check that we have a valid URL and if not we do not change the
        // editing mode.
        if (!isFolder) {
          var urlInput = this.querySelector('.url input');
          var newUrl = urlInput.value;
          if (!newUrl) {
            cr.dispatchSimpleEvent(this, 'canceledit', true);
            return;
          }

          newUrl = getValidURL(urlInput);
          if (!newUrl) {
            // In case the item was removed before getting here we should
            // not alert.
            if (listItem.parentNode) {
              // Select the item again.
              var dataModel = this.parentNode.dataModel;
              var index = dataModel.indexOf(this.bookmarkNode);
              var sm = this.parentNode.selectionModel;
              sm.selectedIndex = sm.leadIndex = sm.anchorIndex = index;

              alert(loadTimeData.getString('invalid_url'));
            }
            urlInput.focus();
            urlInput.select();
            return;
          }
          urlEl.textContent = this.bookmarkNode.url = newUrl;
        }

        this.removeAttribute('editing');
        this.draggable = true;

        labelInput = this.querySelector('.label input');
        var newLabel = labelInput.value;
        labelTextEl.textContent = this.bookmarkNode.title = newLabel;

        if (isFolder) {
          if (newLabel != title) {
            cr.dispatchSimpleEvent(this, 'rename', true);
          }
        } else if (newLabel != title || newUrl != url) {
          cr.dispatchSimpleEvent(this, 'edit', true);
        }
      }
    }
  };

  return {
    BookmarkList: BookmarkList,
    list: /** @type {Element} */(null),  // Set when decorated.
  };
});
