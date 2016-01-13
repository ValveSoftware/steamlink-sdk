// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('dnd', function() {
  'use strict';

  /** @const */ var BookmarkList = bmm.BookmarkList;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var TreeItem = cr.ui.TreeItem;

  /**
   * Enumeration of valid drop locations relative to an element. These are
   * bit masks to allow combining multiple locations in a single value.
   * @enum {number}
   * @const
   */
  var DropPosition = {
    NONE: 0,
    ABOVE: 1,
    ON: 2,
    BELOW: 4
  };

  /**
   * @type {Object} Drop information calculated in |handleDragOver|.
   */
  var dropDestination = null;

  /**
    * @type {number} Timer id used to help minimize flicker.
    */
  var removeDropIndicatorTimer;

  /**
    * The element currently targeted by a touch.
    * @type {Element}
    */
  var currentTouchTarget;

  /**
    * The element that had a style applied it to indicate the drop location.
    * This is used to easily remove the style when necessary.
    * @type {Element}
    */
  var lastIndicatorElement;

  /**
    * The style that was applied to indicate the drop location.
    * @type {string}
    */
  var lastIndicatorClassName;

  var dropIndicator = {
    /**
     * Applies the drop indicator style on the target element and stores that
     * information to easily remove the style in the future.
     */
    addDropIndicatorStyle: function(indicatorElement, position) {
      var indicatorStyleName = position == DropPosition.ABOVE ? 'drag-above' :
                               position == DropPosition.BELOW ? 'drag-below' :
                               'drag-on';

      lastIndicatorElement = indicatorElement;
      lastIndicatorClassName = indicatorStyleName;

      indicatorElement.classList.add(indicatorStyleName);
    },

    /**
     * Clears the drop indicator style from the last element was the drop target
     * so the drop indicator is no longer for that element.
     */
    removeDropIndicatorStyle: function() {
      if (!lastIndicatorElement || !lastIndicatorClassName)
        return;
      lastIndicatorElement.classList.remove(lastIndicatorClassName);
      lastIndicatorElement = null;
      lastIndicatorClassName = null;
    },

    /**
      * Displays the drop indicator on the current drop target to give the
      * user feedback on where the drop will occur.
      */
    update: function(dropDest) {
      window.clearTimeout(removeDropIndicatorTimer);

      var indicatorElement = dropDest.element;
      var position = dropDest.position;
      if (dropDest.element instanceof BookmarkList) {
        // For an empty bookmark list use 'drop-above' style.
        position = DropPosition.ABOVE;
      } else if (dropDest.element instanceof TreeItem) {
        indicatorElement = indicatorElement.querySelector('.tree-row');
      }
      dropIndicator.removeDropIndicatorStyle();
      dropIndicator.addDropIndicatorStyle(indicatorElement, position);
    },

    /**
     * Stop displaying the drop indicator.
     */
    finish: function() {
      // The use of a timeout is in order to reduce flickering as we move
      // between valid drop targets.
      window.clearTimeout(removeDropIndicatorTimer);
      removeDropIndicatorTimer = window.setTimeout(function() {
        dropIndicator.removeDropIndicatorStyle();
      }, 100);
    }
  };

  /**
    * Delay for expanding folder when pointer hovers on folder in tree view in
    * milliseconds.
    * @type {number}
    * @const
    */
  // TODO(yosin): EXPAND_FOLDER_DELAY should follow system settings. 400ms is
  // taken from Windows default settings.
  var EXPAND_FOLDER_DELAY = 400;

  /**
    * The timestamp when the mouse was over a folder during a drag operation.
    * Used to open the hovered folder after a certain time.
    * @type {number}
    */
  var lastHoverOnFolderTimeStamp = 0;

  /**
    * Expand a folder if the user has hovered for longer than the specified
    * time during a drag action.
    */
  function updateAutoExpander(eventTimeStamp, overElement) {
    // Expands a folder in tree view when pointer hovers on it longer than
    // EXPAND_FOLDER_DELAY.
    var hoverOnFolderTimeStamp = lastHoverOnFolderTimeStamp;
    lastHoverOnFolderTimeStamp = 0;
    if (hoverOnFolderTimeStamp) {
      if (eventTimeStamp - hoverOnFolderTimeStamp >= EXPAND_FOLDER_DELAY)
        overElement.expanded = true;
      else
        lastHoverOnFolderTimeStamp = hoverOnFolderTimeStamp;
    } else if (overElement instanceof TreeItem &&
                bmm.isFolder(overElement.bookmarkNode) &&
                overElement.hasChildren &&
                !overElement.expanded) {
      lastHoverOnFolderTimeStamp = eventTimeStamp;
    }
  }

  /**
    * Stores the information about the bookmark and folders being dragged.
    * @type {Object}
    */
  var dragData = null;
  var dragInfo = {
    handleChromeDragEnter: function(newDragData) {
      dragData = newDragData;
    },
    clearDragData: function() {
      dragData = null;
    },
    isDragValid: function() {
      return !!dragData;
    },
    isSameProfile: function() {
      return dragData && dragData.sameProfile;
    },
    isDraggingFolders: function() {
      return dragData && dragData.elements.some(function(node) {
        return !node.url;
      });
    },
    isDraggingBookmark: function(bookmarkId) {
      return dragData && dragData.elements.some(function(node) {
        return node.id == bookmarkId;
      });
    },
    isDraggingChildBookmark: function(folderId) {
      return dragData && dragData.elements.some(function(node) {
        return node.parentId == folderId;
      });
    },
    isDraggingFolderToDescendant: function(bookmarkNode) {
      return dragData && dragData.elements.some(function(node) {
        var dragFolder = bmm.treeLookup[node.id];
        var dragFolderNode = dragFolder && dragFolder.bookmarkNode;
        return dragFolderNode && bmm.contains(dragFolderNode, bookmarkNode);
      });
    }
  };

  /**
   * External function to select folders or bookmarks after a drop action.
   * @type {function}
   */
  var selectItemsAfterUserAction = null;

  function getBookmarkElement(el) {
    while (el && !el.bookmarkNode) {
      el = el.parentNode;
    }
    return el;
  }

  // If we are over the list and the list is showing search result, we cannot
  // drop.
  function isOverSearch(overElement) {
    return list.isSearch() && list.contains(overElement);
  }

  /**
   * Determines the valid drop positions for the given target element.
   * @param {!HTMLElement} overElement The element that we are currently
   *     dragging over.
   * @return {DropPosition} An bit field enumeration of valid drop locations.
   */
  function calculateValidDropTargets(overElement) {
    // Don't allow dropping if there is an ephemeral item being edited.
    if (list.hasEphemeral())
      return DropPosition.NONE;

    if (!dragInfo.isDragValid() || isOverSearch(overElement))
      return DropPosition.NONE;

    if (dragInfo.isSameProfile() &&
        (dragInfo.isDraggingBookmark(overElement.bookmarkNode.id) ||
         dragInfo.isDraggingFolderToDescendant(overElement.bookmarkNode))) {
      return DropPosition.NONE;
    }

    var canDropInfo = calculateDropAboveBelow(overElement);
    if (canDropOn(overElement))
      canDropInfo |= DropPosition.ON;

    return canDropInfo;
  }

  function calculateDropAboveBelow(overElement) {
    if (overElement instanceof BookmarkList)
      return DropPosition.NONE;

    // We cannot drop between Bookmarks bar and Other bookmarks.
    if (overElement.bookmarkNode.parentId == bmm.ROOT_ID)
      return DropPosition.NONE;

    var isOverTreeItem = overElement instanceof TreeItem;
    var isOverExpandedTree = isOverTreeItem && overElement.expanded;
    var isDraggingFolders = dragInfo.isDraggingFolders();

    // We can only drop between items in the tree if we have any folders.
    if (isOverTreeItem && !isDraggingFolders)
      return DropPosition.NONE;

    // When dragging from a different profile we do not need to consider
    // conflicts between the dragged items and the drop target.
    if (!dragInfo.isSameProfile()) {
      // Don't allow dropping below an expanded tree item since it is confusing
      // to the user anyway.
      return isOverExpandedTree ? DropPosition.ABOVE :
                                  (DropPosition.ABOVE | DropPosition.BELOW);
    }

    var resultPositions = DropPosition.NONE;

    // Cannot drop above if the item above is already in the drag source.
    var previousElem = overElement.previousElementSibling;
    if (!previousElem || !dragInfo.isDraggingBookmark(previousElem.bookmarkId))
      resultPositions |= DropPosition.ABOVE;

    // Don't allow dropping below an expanded tree item since it is confusing
    // to the user anyway.
    if (isOverExpandedTree)
      return resultPositions;

    // Cannot drop below if the item below is already in the drag source.
    var nextElement = overElement.nextElementSibling;
    if (!nextElement || !dragInfo.isDraggingBookmark(nextElement.bookmarkId))
      resultPositions |= DropPosition.BELOW;

    return resultPositions;
  }

  /**
   * Determine whether we can drop the dragged items on the drop target.
   * @param {!HTMLElement} overElement The element that we are currently
   *     dragging over.
   * @return {boolean} Whether we can drop the dragged items on the drop
   *     target.
   */
  function canDropOn(overElement) {
    // We can only drop on a folder.
    if (!bmm.isFolder(overElement.bookmarkNode))
      return false;

    if (!dragInfo.isSameProfile())
      return true;

    if (overElement instanceof BookmarkList) {
      // We are trying to drop an item past the last item. This is
      // only allowed if dragged item is different from the last item
      // in the list.
      var listItems = list.items;
      var len = listItems.length;
      if (!len || !dragInfo.isDraggingBookmark(listItems[len - 1].bookmarkId))
        return true;
    }

    return !dragInfo.isDraggingChildBookmark(overElement.bookmarkNode.id);
  }

  /**
   * Callback for the dragstart event.
   * @param {Event} e The dragstart event.
   */
  function handleDragStart(e) {
    // Determine the selected bookmarks.
    var target = e.target;
    var draggedNodes = [];
    var isFromTouch = target == currentTouchTarget;

    if (target instanceof ListItem) {
      // Use selected items.
      draggedNodes = target.parentNode.selectedItems;
    } else if (target instanceof TreeItem) {
      draggedNodes.push(target.bookmarkNode);
    }

    // We manage starting the drag by using the extension API.
    e.preventDefault();

    // Do not allow dragging if there is an ephemeral item being edited at the
    // moment.
    if (list.hasEphemeral())
      return;

    if (draggedNodes.length) {
      // If we are dragging a single link, we can do the *Link* effect.
      // Otherwise, we only allow copy and move.
      e.dataTransfer.effectAllowed = draggedNodes.length == 1 &&
          !bmm.isFolder(draggedNodes[0]) ? 'copyMoveLink' : 'copyMove';

      chrome.bookmarkManagerPrivate.startDrag(draggedNodes.map(function(node) {
        return node.id;
      }), isFromTouch);
    }
  }

  function handleDragEnter(e) {
    e.preventDefault();
  }

  /**
   * Calback for the dragover event.
   * @param {Event} e The dragover event.
   */
  function handleDragOver(e) {
    // Allow DND on text inputs.
    if (e.target.tagName != 'INPUT') {
      // The default operation is to allow dropping links etc to do navigation.
      // We never want to do that for the bookmark manager.
      e.preventDefault();

      // Set to none. This will get set to something if we can do the drop.
      e.dataTransfer.dropEffect = 'none';
    }

    if (!dragInfo.isDragValid())
      return;

    var overElement = getBookmarkElement(e.target) ||
                      (e.target == list ? list : null);
    if (!overElement)
      return;

    updateAutoExpander(e.timeStamp, overElement);

    var canDropInfo = calculateValidDropTargets(overElement);
    if (canDropInfo == DropPosition.NONE)
      return;

    // Now we know that we can drop. Determine if we will drop above, on or
    // below based on mouse position etc.

    dropDestination = calcDropPosition(e.clientY, overElement, canDropInfo);
    if (!dropDestination) {
      e.dataTransfer.dropEffect = 'none';
      return;
    }

    e.dataTransfer.dropEffect = dragInfo.isSameProfile() ? 'move' : 'copy';
    dropIndicator.update(dropDestination);
  }

  /**
   * This function determines where the drop will occur relative to the element.
   * @return {?Object} If no valid drop position is found, null, otherwise
   *     an object containing the following parameters:
   *       element - The target element that will receive the drop.
   *       position - A |DropPosition| relative to the |element|.
   */
  function calcDropPosition(elementClientY, overElement, canDropInfo) {
    if (overElement instanceof BookmarkList) {
      // Dropping on the BookmarkList either means dropping below the last
      // bookmark element or on the list itself if it is empty.
      var length = overElement.items.length;
      if (length)
        return {
          element: overElement.getListItemByIndex(length - 1),
          position: DropPosition.BELOW
        };
      return {element: overElement, position: DropPosition.ON};
    }

    var above = canDropInfo & DropPosition.ABOVE;
    var below = canDropInfo & DropPosition.BELOW;
    var on = canDropInfo & DropPosition.ON;
    var rect = overElement.getBoundingClientRect();
    var yRatio = (elementClientY - rect.top) / rect.height;

    if (above && (yRatio <= .25 || yRatio <= .5 && (!below || !on)))
      return {element: overElement, position: DropPosition.ABOVE};
    if (below && (yRatio > .75 || yRatio > .5 && (!above || !on)))
      return {element: overElement, position: DropPosition.BELOW};
    if (on)
      return {element: overElement, position: DropPosition.ON};
    return null;
  }

  function calculateDropInfo(eventTarget, dropDestination) {
    if (!dropDestination || !dragInfo.isDragValid())
      return null;

    var dropPos = dropDestination.position;
    var relatedNode = dropDestination.element.bookmarkNode;
    var dropInfoResult = {
        selectTarget: null,
        selectedTreeId: -1,
        parentId: dropPos == DropPosition.ON ? relatedNode.id :
                                               relatedNode.parentId,
        index: -1,
        relatedIndex: -1
      };

    // Try to find the index in the dataModel so we don't have to always keep
    // the index for the list items up to date.
    var overElement = getBookmarkElement(eventTarget);
    if (overElement instanceof ListItem) {
      dropInfoResult.relatedIndex =
          overElement.parentNode.dataModel.indexOf(relatedNode);
      dropInfoResult.selectTarget = list;
    } else if (overElement instanceof BookmarkList) {
      dropInfoResult.relatedIndex = overElement.dataModel.length - 1;
      dropInfoResult.selectTarget = list;
    } else {
      // Tree
      dropInfoResult.relatedIndex = relatedNode.index;
      dropInfoResult.selectTarget = tree;
      dropInfoResult.selectedTreeId =
          tree.selectedItem ? tree.selectedItem.bookmarkId : null;
    }

    if (dropPos == DropPosition.ABOVE)
      dropInfoResult.index = dropInfoResult.relatedIndex;
    else if (dropPos == DropPosition.BELOW)
      dropInfoResult.index = dropInfoResult.relatedIndex + 1;

    return dropInfoResult;
  }

  function handleDragLeave(e) {
    dropIndicator.finish();
  }

  function handleDrop(e) {
    var dropInfo = calculateDropInfo(e.target, dropDestination);
    if (dropInfo) {
      selectItemsAfterUserAction(dropInfo.selectTarget,
                                 dropInfo.selectedTreeId);
      if (dropInfo.index != -1)
        chrome.bookmarkManagerPrivate.drop(dropInfo.parentId, dropInfo.index);
      else
        chrome.bookmarkManagerPrivate.drop(dropInfo.parentId);

      e.preventDefault();
    }
    dropDestination = null;
    dropIndicator.finish();
  }

  function setCurrentTouchTarget(e) {
    // Only set a new target for a single touch point.
    if (e.touches.length == 1)
      currentTouchTarget = getBookmarkElement(e.target);
  }

  function clearCurrentTouchTarget(e) {
    if (getBookmarkElement(e.target) == currentTouchTarget)
      currentTouchTarget = null;
  }

  function clearDragData() {
    dragInfo.clearDragData();
    dropDestination = null;
  }

  function init(selectItemsAfterUserActionFunction) {
    function deferredClearData() {
      setTimeout(clearDragData);
    }

    selectItemsAfterUserAction = selectItemsAfterUserActionFunction;

    document.addEventListener('dragstart', handleDragStart);
    document.addEventListener('dragenter', handleDragEnter);
    document.addEventListener('dragover', handleDragOver);
    document.addEventListener('dragleave', handleDragLeave);
    document.addEventListener('drop', handleDrop);
    document.addEventListener('dragend', deferredClearData);
    document.addEventListener('mouseup', deferredClearData);
    document.addEventListener('mousedown', clearCurrentTouchTarget);
    document.addEventListener('touchcancel', clearCurrentTouchTarget);
    document.addEventListener('touchend', clearCurrentTouchTarget);
    document.addEventListener('touchstart', setCurrentTouchTarget);

    chrome.bookmarkManagerPrivate.onDragEnter.addListener(
        dragInfo.handleChromeDragEnter);
    chrome.bookmarkManagerPrivate.onDragLeave.addListener(deferredClearData);
    chrome.bookmarkManagerPrivate.onDrop.addListener(deferredClearData);
  }
  return {init: init};
});
