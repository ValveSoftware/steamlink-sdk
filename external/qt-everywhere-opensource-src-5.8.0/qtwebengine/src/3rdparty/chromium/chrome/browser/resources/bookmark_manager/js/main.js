// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @const */ var BookmarkList = bmm.BookmarkList;
/** @const */ var BookmarkTree = bmm.BookmarkTree;
/** @const */ var Command = cr.ui.Command;
/** @const */ var LinkKind = cr.LinkKind;
/** @const */ var ListItem = cr.ui.ListItem;
/** @const */ var Menu = cr.ui.Menu;
/** @const */ var MenuButton = cr.ui.MenuButton;
/** @const */ var Splitter = cr.ui.Splitter;
/** @const */ var TreeItem = cr.ui.TreeItem;

/**
 * An array containing the BookmarkTreeNodes that were deleted in the last
 * deletion action. This is used for implementing undo.
 * @type {?{nodes: Array<Array<BookmarkTreeNode>>, target: EventTarget}}
 */
var lastDeleted;

/**
 *
 * Holds the last DOMTimeStamp when mouse pointer hovers on folder in tree
 * view. Zero means pointer doesn't hover on folder.
 * @type {number}
 */
var lastHoverOnFolderTimeStamp = 0;

/**
 * Holds a function that will undo that last action, if global undo is enabled.
 * @type {Function}
 */
var performGlobalUndo;

/**
 * Holds a link controller singleton. Use getLinkController() rarther than
 * accessing this variabie.
 * @type {cr.LinkController}
 */
var linkController;

/**
 * New Windows are not allowed in Windows 8 metro mode.
 */
var canOpenNewWindows = true;

/**
 * Incognito mode availability can take the following values: ,
 *   - 'enabled' for when both normal and incognito modes are available;
 *   - 'disabled' for when incognito mode is disabled;
 *   - 'forced' for when incognito mode is forced (normal mode is unavailable).
 */
var incognitoModeAvailability = 'enabled';

/**
 * Whether bookmarks can be modified.
 * @type {boolean}
 */
var canEdit = true;

/**
 * @type {TreeItem}
 * @const
 */
var searchTreeItem = new TreeItem({
  bookmarkId: 'q='
});

/**
 * Command shortcut mapping.
 * @const
 */
var commandShortcutMap = cr.isMac ? {
  'edit': 'Enter',
  // On Mac we also allow Meta+Backspace.
  'delete': 'U+007F  U+0008 Meta-U+0008',
  'open-in-background-tab': 'Meta-Enter',
  'open-in-new-tab': 'Shift-Meta-Enter',
  'open-in-same-window': 'Meta-Down',
  'open-in-new-window': 'Shift-Enter',
  'rename-folder': 'Enter',
  // Global undo is Command-Z. It is not in any menu.
  'undo': 'Meta-U+005A',
} : {
  'edit': 'F2',
  'delete': 'U+007F',
  'open-in-background-tab': 'Ctrl-Enter',
  'open-in-new-tab': 'Shift-Ctrl-Enter',
  'open-in-same-window': 'Enter',
  'open-in-new-window': 'Shift-Enter',
  'rename-folder': 'F2',
  // Global undo is Ctrl-Z. It is not in any menu.
  'undo': 'Ctrl-U+005A',
};

/**
 * Mapping for folder id to suffix of UMA. These names will be appeared
 * after "BookmarkManager_NavigateTo_" in UMA dashboard.
 * @const
 */
var folderMetricsNameMap = {
  '1': 'BookmarkBar',
  '2': 'Other',
  '3': 'Mobile',
  'q=': 'Search',
  'subfolder': 'SubFolder',
};

/**
 * Adds an event listener to a node that will remove itself after firing once.
 * @param {!Element} node The DOM node to add the listener to.
 * @param {string} name The name of the event listener to add to.
 * @param {function(Event)} handler Function called when the event fires.
 */
function addOneShotEventListener(node, name, handler) {
  var f = function(e) {
    handler(e);
    node.removeEventListener(name, f);
  };
  node.addEventListener(name, f);
}

// Get the localized strings from the backend via bookmakrManagerPrivate API.
function loadLocalizedStrings(data) {
  // The strings may contain & which we need to strip.
  for (var key in data) {
    data[key] = data[key].replace(/&/, '');
  }

  loadTimeData.data = data;
  i18nTemplate.process(document, loadTimeData);

  searchTreeItem.label = loadTimeData.getString('search');
  searchTreeItem.icon = isRTL() ? 'images/bookmark_manager_search_rtl.png' :
                                  'images/bookmark_manager_search.png';
}

/**
 * Updates the location hash to reflect the current state of the application.
 */
function updateHash() {
  window.location.hash = bmm.tree.selectedItem.bookmarkId;
  updateAllCommands();
}

/**
 * Navigates to a bookmark ID.
 * @param {string} id The ID to navigate to.
 * @param {function()=} opt_callback Function called when list view loaded or
 *     displayed specified folder.
 */
function navigateTo(id, opt_callback) {
  window.location.hash = id;

  var sameParent = bmm.list.parentId == id;
  if (!sameParent)
    updateParentId(id);

  updateAllCommands();

  var metricsId = folderMetricsNameMap[id.replace(/^q=.*/, 'q=')] ||
                  folderMetricsNameMap['subfolder'];
  chrome.metricsPrivate.recordUserAction(
      'BookmarkManager_NavigateTo_' + metricsId);

  if (opt_callback) {
    if (sameParent)
      opt_callback();
    else
      addOneShotEventListener(bmm.list, 'load', opt_callback);
  }
}

/**
 * Updates the parent ID of the bookmark list and selects the correct tree item.
 * @param {string} id The id.
 */
function updateParentId(id) {
  // Setting list.parentId fires 'load' event.
  bmm.list.parentId = id;

  // When tree.selectedItem changed, tree view calls navigatTo() then it
  // calls updateHash() when list view displayed specified folder.
  bmm.tree.selectedItem = bmm.treeLookup[id] || bmm.tree.selectedItem;
}

// Process the location hash. This is called by onhashchange and when the page
// is first loaded.
function processHash() {
  var id = window.location.hash.slice(1);
  if (!id) {
    // If we do not have a hash, select first item in the tree.
    id = bmm.tree.items[0].bookmarkId;
  }

  var valid = false;
  if (/^e=/.test(id)) {
    id = id.slice(2);

    // If hash contains e=, edit the item specified.
    chrome.bookmarks.get(id, function(bookmarkNodes) {
      // Verify the node to edit is a valid node.
      if (!bookmarkNodes || bookmarkNodes.length != 1)
        return;
      var bookmarkNode = bookmarkNodes[0];

      // After the list reloads, edit the desired bookmark.
      var editBookmark = function() {
        var index = bmm.list.dataModel.findIndexById(bookmarkNode.id);
        if (index != -1) {
          var sm = bmm.list.selectionModel;
          sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
          scrollIntoViewAndMakeEditable(index);
        }
      };

      var parentId = assert(bookmarkNode.parentId);
      navigateTo(parentId, editBookmark);
    });

    // We handle the two cases of navigating to the bookmark to be edited
    // above. Don't run the standard navigation code below.
    return;
  } else if (/^q=/.test(id)) {
    // In case we got a search hash, update the text input and the
    // bmm.treeLookup to use the new id.
    setSearch(id.slice(2));
    valid = true;
  }

  // Navigate to bookmark 'id' (which may be a query of the form q=query).
  if (valid) {
    updateParentId(id);
  } else {
    // We need to verify that this is a correct ID.
    chrome.bookmarks.get(id, function(items) {
      if (items && items.length == 1)
        updateParentId(id);
    });
  }
}

// Activate is handled by the open-in-same-window-command.
function handleDoubleClickForList(e) {
  if (e.button == 0)
    $('open-in-same-window-command').execute();
}

// The list dispatches an event when the user clicks on the URL or the Show in
// folder part.
function handleUrlClickedForList(e) {
  getLinkController().openUrlFromEvent(e.url, e.originalEvent);
  chrome.bookmarkManagerPrivate.recordLaunch();
}

function handleSearch(e) {
  setSearch(this.value);
}

/**
 * Navigates to the search results for the search text.
 * @param {string} searchText The text to search for.
 */
function setSearch(searchText) {
  if (searchText) {
    // Only update search item if we have a search term. We never want the
    // search item to be for an empty search.
    delete bmm.treeLookup[searchTreeItem.bookmarkId];
    var id = searchTreeItem.bookmarkId = 'q=' + searchText;
    bmm.treeLookup[searchTreeItem.bookmarkId] = searchTreeItem;
  }

  var input = $('term');
  // Do not update the input if the user is actively using the text input.
  if (document.activeElement != input)
    input.value = searchText;

  if (searchText) {
    bmm.tree.add(searchTreeItem);
    bmm.tree.selectedItem = searchTreeItem;
  } else {
    // Go "home".
    bmm.tree.selectedItem = bmm.tree.items[0];
    id = bmm.tree.selectedItem.bookmarkId;
  }

  navigateTo(id);
}

/**
 * This returns the user visible path to the folder where the bookmark is
 * located.
 * @param {number} parentId The ID of the parent folder.
 * @return {string|undefined} The path to the the bookmark,
 */
function getFolder(parentId) {
  var parentNode = bmm.tree.getBookmarkNodeById(parentId);
  if (parentNode) {
    var s = parentNode.title;
    if (parentNode.parentId != bmm.ROOT_ID) {
      return getFolder(parentNode.parentId) + '/' + s;
    }
    return s;
  }
}

function handleLoadForTree(e) {
  processHash();
}

/**
 * Returns a promise for all the URLs in the {@code nodes} and the direct
 * children of {@code nodes}.
 * @param {!Array<BookmarkTreeNode>} nodes .
 * @return {!Promise<Array<string>>} .
 */
function getAllUrls(nodes) {
  var urls = [];

  // Adds the node and all its direct children.
  // TODO(deepak.m1): Here node should exist. When we delete the nodes then
  // datamodel gets updated but still it shows deleted items as selected items
  // and accessing those nodes throws chrome.runtime.lastError. This cause
  // undefined value for node. Please refer https://crbug.com/480935.
  function addNodes(node) {
    if (!node || node.id == 'new')
      return;

    if (node.children) {
      node.children.forEach(function(child) {
        if (!bmm.isFolder(child))
          urls.push(child.url);
      });
    } else {
      urls.push(node.url);
    }
  }

  // Get a future promise for the nodes.
  var promises = nodes.map(function(node) {
    if (bmm.isFolder(assert(node)))
      return bmm.loadSubtree(node.id);
    // Not a folder so we already have all the data we need.
    return Promise.resolve(node);
  });

  return Promise.all(promises).then(function(nodes) {
    nodes.forEach(addNodes);
    return urls;
  });
}

/**
 * Returns the nodes (non recursive) to use for the open commands.
 * @param {HTMLElement} target
 * @return {!Array<BookmarkTreeNode>}
 */
function getNodesForOpen(target) {
  if (target == bmm.tree) {
    if (bmm.tree.selectedItem != searchTreeItem)
      return bmm.tree.selectedFolders;
    // Fall through to use all nodes in the list.
  } else {
    var items = bmm.list.selectedItems;
    if (items.length)
      return items;
  }

  // The list starts off with a null dataModel. We can get here during startup.
  if (!bmm.list.dataModel)
    return [];

  // Return an array based on the dataModel.
  return bmm.list.dataModel.slice();
}

/**
 * Returns a promise that will contain all URLs of all the selected bookmarks
 * and the nested bookmarks for use with the open commands.
 * @param {HTMLElement} target The target list or tree.
 * @return {Promise<Array<string>>} .
 */
function getUrlsForOpenCommands(target) {
  return getAllUrls(getNodesForOpen(target));
}

function notNewNode(node) {
  return node.id != 'new';
}

/**
 * Helper function that updates the canExecute and labels for the open-like
 * commands.
 * @param {!cr.ui.CanExecuteEvent} e The event fired by the command system.
 * @param {!cr.ui.Command} command The command we are currently processing.
 * @param {string} singularId The string id of singular form of the menu label.
 * @param {string} pluralId The string id of menu label if the singular form is
       not used.
 * @param {boolean} commandDisabled Whether the menu item should be disabled
       no matter what bookmarks are selected.
 */
function updateOpenCommand(e, command, singularId, pluralId, commandDisabled) {
  if (singularId) {
    // The command label reflects the selection which might not reflect
    // how many bookmarks will be opened. For example if you right click an
    // empty area in a folder with 1 bookmark the text should still say "all".
    var selectedNodes = getSelectedBookmarkNodes(e.target).filter(notNewNode);
    var singular = selectedNodes.length == 1 && !bmm.isFolder(selectedNodes[0]);
    command.label = loadTimeData.getString(singular ? singularId : pluralId);
  }

  if (commandDisabled) {
    command.disabled = true;
    e.canExecute = false;
    return;
  }

  getUrlsForOpenCommands(assertInstanceof(e.target, HTMLElement)).then(
      function(urls) {
    var disabled = !urls.length;
    command.disabled = disabled;
    e.canExecute = !disabled;
  });
}

/**
 * Calls the backend to figure out if we can paste the clipboard into the active
 * folder.
 * @param {Function=} opt_f Function to call after the state has been updated.
 */
function updatePasteCommand(opt_f) {
  function update(commandId, canPaste) {
    $(commandId).disabled = !canPaste;
  }

  var promises = [];

  // The folders menu.
  // We can not paste into search item in tree.
  if (bmm.tree.selectedItem && bmm.tree.selectedItem != searchTreeItem) {
    promises.push(new Promise(function(resolve) {
      var id = bmm.tree.selectedItem.bookmarkId;
      chrome.bookmarkManagerPrivate.canPaste(id, function(canPaste) {
        update('paste-from-folders-menu-command', canPaste);
        resolve(canPaste);
      });
    }));
  } else {
    // Tree's not loaded yet.
    update('paste-from-folders-menu-command', false);
  }

  // The organize menu.
  var listId = bmm.list.parentId;
  if (bmm.list.isSearch() || !listId) {
    // We cannot paste into search view or the list isn't ready.
    update('paste-from-organize-menu-command', false);
  } else {
    promises.push(new Promise(function(resolve) {
      chrome.bookmarkManagerPrivate.canPaste(listId, function(canPaste) {
        update('paste-from-organize-menu-command', canPaste);
        resolve(canPaste);
      });
    }));
  }

  Promise.all(promises).then(function() {
    var cmd;
    if (document.activeElement == bmm.list)
      cmd = 'paste-from-organize-menu-command';
    else if (document.activeElement == bmm.tree)
      cmd = 'paste-from-folders-menu-command';

    if (cmd)
      update('paste-from-context-menu-command', !$(cmd).disabled);

    if (opt_f) opt_f();
  });
}

function handleCanExecuteForSearchBox(e) {
  var command = e.command;
  switch (command.id) {
    case 'delete-command':
    case 'undo-command':
      // Pass the delete and undo commands through
      // (fixes http://crbug.com/278112).
      e.canExecute = false;
      break;
  }
}

function handleCanExecuteForDocument(e) {
  var command = e.command;
  switch (command.id) {
    case 'import-menu-command':
      e.canExecute = canEdit;
      break;

    case 'export-menu-command':
      // We can always execute the export-menu command.
      e.canExecute = true;
      break;

    case 'sort-command':
      e.canExecute = !bmm.list.isSearch() &&
          bmm.list.dataModel && bmm.list.dataModel.length > 1 &&
          !isUnmodifiable(bmm.tree.getBookmarkNodeById(bmm.list.parentId));
      break;

    case 'undo-command':
      // Because the global undo command has no visible UI, always enable it,
      // and just make it a no-op if undo is not possible.
      e.canExecute = true;
      break;

    default:
      canExecuteForList(e);
      if (!e.defaultPrevented)
        canExecuteForTree(e);
      break;
  }
}

/**
 * Helper function for handling canExecute for the list and the tree.
 * @param {!cr.ui.CanExecuteEvent} e Can execute event object.
 * @param {boolean} isSearch Whether the user is trying to do a command on
 *     search.
 */
function canExecuteShared(e, isSearch) {
  var command = e.command;
  switch (command.id) {
    case 'paste-from-folders-menu-command':
    case 'paste-from-organize-menu-command':
    case 'paste-from-context-menu-command':
      updatePasteCommand();
      break;

    case 'add-new-bookmark-command':
    case 'new-folder-command':
    case 'new-folder-from-folders-menu-command':
      var parentId = computeParentFolderForNewItem();
      var unmodifiable = isUnmodifiable(
          bmm.tree.getBookmarkNodeById(parentId));
      e.canExecute = !isSearch && canEdit && !unmodifiable;
      break;

    case 'open-in-new-tab-command':
      updateOpenCommand(e, command, 'open_in_new_tab', 'open_all', false);
      break;

    case 'open-in-background-tab-command':
      updateOpenCommand(e, command, '', '', false);
      break;

    case 'open-in-new-window-command':
      updateOpenCommand(e, command,
          'open_in_new_window', 'open_all_new_window',
          // Disabled when incognito is forced.
          incognitoModeAvailability == 'forced' || !canOpenNewWindows);
      break;

    case 'open-incognito-window-command':
      updateOpenCommand(e, command,
          'open_incognito', 'open_all_incognito',
          // Not available when incognito is disabled.
          incognitoModeAvailability == 'disabled');
      break;

    case 'undo-delete-command':
      e.canExecute = !!lastDeleted;
      break;
  }
}

/**
 * Helper function for handling canExecute for the list and document.
 * @param {!cr.ui.CanExecuteEvent} e Can execute event object.
 */
function canExecuteForList(e) {
  function hasSelected() {
    return !!bmm.list.selectedItem;
  }

  function hasSingleSelected() {
    return bmm.list.selectedItems.length == 1;
  }

  function canCopyItem(item) {
    return item.id != 'new';
  }

  function canCopyItems() {
    var selectedItems = bmm.list.selectedItems;
    return selectedItems && selectedItems.some(canCopyItem);
  }

  function isSearch() {
    return bmm.list.isSearch();
  }

  var command = e.command;
  switch (command.id) {
    case 'rename-folder-command':
      // Show rename if a single folder is selected.
      var items = bmm.list.selectedItems;
      if (items.length != 1) {
        e.canExecute = false;
        command.hidden = true;
      } else {
        var isFolder = bmm.isFolder(items[0]);
        e.canExecute = isFolder && canEdit && !hasUnmodifiable(items);
        command.hidden = !isFolder;
      }
      break;

    case 'edit-command':
      // Show the edit command if not a folder.
      var items = bmm.list.selectedItems;
      if (items.length != 1) {
        e.canExecute = false;
        command.hidden = false;
      } else {
        var isFolder = bmm.isFolder(items[0]);
        e.canExecute = !isFolder && canEdit && !hasUnmodifiable(items);
        command.hidden = isFolder;
      }
      break;

    case 'show-in-folder-command':
      e.canExecute = isSearch() && hasSingleSelected();
      break;

    case 'delete-command':
    case 'cut-command':
      e.canExecute = canCopyItems() && canEdit &&
          !hasUnmodifiable(bmm.list.selectedItems);
      break;

    case 'copy-command':
      e.canExecute = canCopyItems();
      break;

    case 'open-in-same-window-command':
      e.canExecute = (e.target == bmm.list) && hasSelected();
      break;

    default:
      canExecuteShared(e, isSearch());
  }
}

// Update canExecute for the commands when the list is the active element.
function handleCanExecuteForList(e) {
  if (e.target != bmm.list) return;
  canExecuteForList(e);
}

// Update canExecute for the commands when the tree is the active element.
function handleCanExecuteForTree(e) {
  if (e.target != bmm.tree) return;
  canExecuteForTree(e);
}

function canExecuteForTree(e) {
  function hasSelected() {
    return !!bmm.tree.selectedItem;
  }

  function isSearch() {
    return bmm.tree.selectedItem == searchTreeItem;
  }

  function isTopLevelItem() {
    return bmm.tree.selectedItem &&
           bmm.tree.selectedItem.parentNode == bmm.tree;
  }

  var command = e.command;
  switch (command.id) {
    case 'rename-folder-command':
    case 'rename-folder-from-folders-menu-command':
      command.hidden = false;
      e.canExecute = hasSelected() && !isTopLevelItem() && canEdit &&
          !hasUnmodifiable(bmm.tree.selectedFolders);
      break;

    case 'edit-command':
      command.hidden = true;
      e.canExecute = false;
      break;

    case 'delete-command':
    case 'delete-from-folders-menu-command':
    case 'cut-command':
    case 'cut-from-folders-menu-command':
      e.canExecute = hasSelected() && !isTopLevelItem() && canEdit &&
          !hasUnmodifiable(bmm.tree.selectedFolders);
      break;

    case 'copy-command':
    case 'copy-from-folders-menu-command':
      e.canExecute = hasSelected() && !isTopLevelItem();
      break;

    case 'undo-delete-from-folders-menu-command':
      e.canExecute = lastDeleted && lastDeleted.target == bmm.tree;
      break;

    default:
      canExecuteShared(e, isSearch());
  }
}

/**
 * Update the canExecute state of all the commands.
 */
function updateAllCommands() {
  var commands = document.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++) {
    commands[i].canExecuteChange();
  }
}

function updateEditingCommands() {
  var editingCommands = [
    'add-new-bookmark',
    'cut',
    'cut-from-folders-menu',
    'delete',
    'edit',
    'new-folder',
    'paste-from-context-menu',
    'paste-from-folders-menu',
    'paste-from-organize-menu',
    'rename-folder',
    'sort',
  ];

  chrome.bookmarkManagerPrivate.canEdit(function(result) {
    if (result != canEdit) {
      canEdit = result;
      editingCommands.forEach(function(baseId) {
        $(baseId + '-command').canExecuteChange();
      });
    }
  });
}

function handleChangeForTree(e) {
  navigateTo(bmm.tree.selectedItem.bookmarkId);
}

function handleMenuButtonClicked(e) {
  updateEditingCommands();

  if (e.currentTarget.id == 'folders-menu') {
    $('copy-from-folders-menu-command').canExecuteChange();
    $('undo-delete-from-folders-menu-command').canExecuteChange();
  } else {
    $('copy-command').canExecuteChange();
  }
}

function handleRename(e) {
  var item = e.target;
  var bookmarkNode = item.bookmarkNode;
  chrome.bookmarks.update(bookmarkNode.id, {title: item.label});
  performGlobalUndo = null;  // This can't be undone, so disable global undo.
}

function handleEdit(e) {
  var item = e.target;
  var bookmarkNode = item.bookmarkNode;
  var context = {
    title: bookmarkNode.title
  };
  if (!bmm.isFolder(bookmarkNode))
    context.url = bookmarkNode.url;

  if (bookmarkNode.id == 'new') {
    selectItemsAfterUserAction(/** @type {BookmarkList} */(bmm.list));

    // New page
    context.parentId = bookmarkNode.parentId;
    chrome.bookmarks.create(context, function(node) {
      // A new node was created and will get added to the list due to the
      // handler.
      var dataModel = bmm.list.dataModel;
      var index = dataModel.indexOf(bookmarkNode);
      dataModel.splice(index, 1);

      // Select new item.
      var newIndex = dataModel.findIndexById(node.id);
      if (newIndex != -1) {
        var sm = bmm.list.selectionModel;
        bmm.list.scrollIndexIntoView(newIndex);
        sm.leadIndex = sm.anchorIndex = sm.selectedIndex = newIndex;
      }
    });
  } else {
    // Edit
    chrome.bookmarks.update(bookmarkNode.id, context);
  }
  performGlobalUndo = null;  // This can't be undone, so disable global undo.
}

function handleCancelEdit(e) {
  var item = e.target;
  var bookmarkNode = item.bookmarkNode;
  if (bookmarkNode.id == 'new') {
    var dataModel = bmm.list.dataModel;
    var index = dataModel.findIndexById('new');
    dataModel.splice(index, 1);
  }
}

/**
 * Navigates to the folder that the selected item is in and selects it. This is
 * used for the show-in-folder command.
 */
function showInFolder() {
  var bookmarkNode = bmm.list.selectedItem;
  if (!bookmarkNode)
    return;
  var parentId = bookmarkNode.parentId;

  // After the list is loaded we should select the revealed item.
  function selectItem() {
    var index = bmm.list.dataModel.findIndexById(bookmarkNode.id);
    if (index == -1)
      return;
    var sm = bmm.list.selectionModel;
    sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
    bmm.list.scrollIndexIntoView(index);
  }

  var treeItem = bmm.treeLookup[parentId];
  treeItem.reveal();

  navigateTo(parentId, selectItem);
}

/**
 * @return {!cr.LinkController} The link controller used to open links based on
 *     user clicks and keyboard actions.
 */
function getLinkController() {
  return linkController ||
      (linkController = new cr.LinkController(loadTimeData));
}

/**
 * Returns the selected bookmark nodes of the provided tree or list.
 * If |opt_target| is not provided or null the active element is used.
 * Only call this if the list or the tree is focused.
 * @param {EventTarget=} opt_target The target list or tree.
 * @return {!Array} Array of bookmark nodes.
 */
function getSelectedBookmarkNodes(opt_target) {
  return (opt_target || document.activeElement) == bmm.tree ?
      bmm.tree.selectedFolders : bmm.list.selectedItems;
}

/**
 * @param {EventTarget=} opt_target The target list or tree.
 * @return {!Array<string>} An array of the selected bookmark IDs.
 */
function getSelectedBookmarkIds(opt_target) {
  var selectedNodes = getSelectedBookmarkNodes(opt_target);
  selectedNodes.sort(function(a, b) { return a.index - b.index });
  return selectedNodes.map(function(node) {
    return node.id;
  });
}

/**
 * @param {BookmarkTreeNode} node The node to test.
 * @return {boolean} Whether the given node is unmodifiable.
 */
function isUnmodifiable(node) {
  return !!(node && node.unmodifiable);
}

/**
 * @param {Array<BookmarkTreeNode>} nodes A list of BookmarkTreeNodes.
 * @return {boolean} Whether any of the nodes is managed.
 */
function hasUnmodifiable(nodes) {
  return nodes.some(isUnmodifiable);
}

/**
 * Opens the selected bookmarks.
 * @param {cr.LinkKind} kind The kind of link we want to open.
 * @param {HTMLElement=} opt_eventTarget The target of the user initiated event.
 */
function openBookmarks(kind, opt_eventTarget) {
  // If we have selected any folders, we need to find all the bookmarks one
  // level down. We use multiple async calls to getSubtree instead of getting
  // the whole tree since we would like to minimize the amount of data sent.

  var urlsP = getUrlsForOpenCommands(opt_eventTarget ? opt_eventTarget : null);
  urlsP.then(function(urls) {
    getLinkController().openUrls(assert(urls), kind);
    chrome.bookmarkManagerPrivate.recordLaunch();
  });
}

/**
 * Opens an item in the list.
 */
function openItem() {
  var bookmarkNodes = getSelectedBookmarkNodes();
  // If we double clicked or pressed enter on a single folder, navigate to it.
  if (bookmarkNodes.length == 1 && bmm.isFolder(bookmarkNodes[0]))
    navigateTo(bookmarkNodes[0].id);
  else
    openBookmarks(LinkKind.FOREGROUND_TAB);
}

/**
 * Refreshes search results after delete or undo-delete.
 * This ensures children of deleted folders do not remain in results
 */
function updateSearchResults() {
  if (bmm.list.isSearch())
    bmm.list.reload();
}

/**
 * Deletes the selected bookmarks. The bookmarks are saved in memory in case
 * the user needs to undo the deletion.
 * @param {EventTarget=} opt_target The deleter of bookmarks.
 */
function deleteBookmarks(opt_target) {
  var selectedIds = getSelectedBookmarkIds(opt_target);
  if (!selectedIds.length)
    return;

  var filteredIds = getFilteredSelectedBookmarkIds(opt_target);
  lastDeleted = {nodes: [], target: opt_target || document.activeElement};

  function performDelete() {
    // Only remove filtered ids.
    chrome.bookmarkManagerPrivate.removeTrees(filteredIds);
    $('undo-delete-command').canExecuteChange();
    $('undo-delete-from-folders-menu-command').canExecuteChange();
    performGlobalUndo = undoDelete;
  }

  // First, store information about the bookmarks being deleted.
  // Store all selected ids.
  selectedIds.forEach(function(id) {
    chrome.bookmarks.getSubTree(id, function(results) {
      lastDeleted.nodes.push(results);

      // When all nodes have been saved, perform the deletion.
      if (lastDeleted.nodes.length === selectedIds.length) {
        performDelete();
        updateSearchResults();
      }
    });
  });
}

/**
 * Restores a tree of bookmarks under a specified folder.
 * @param {BookmarkTreeNode} node The node to restore.
 * @param {(string|number)=} opt_parentId If a string is passed, it's the ID of
 *     the folder to restore under. If not specified or a number is passed, the
 *     original parentId of the node will be used.
 */
function restoreTree(node, opt_parentId) {
  var bookmarkInfo = {
    parentId: typeof opt_parentId == 'string' ? opt_parentId : node.parentId,
    title: node.title,
    index: node.index,
    url: node.url
  };

  chrome.bookmarks.create(bookmarkInfo, function(result) {
    if (!result) {
      console.error('Failed to restore bookmark.');
      return;
    }

    if (node.children) {
      // Restore the children using the new ID for this node.
      node.children.forEach(function(child) {
        restoreTree(child, result.id);
      });
    }

    updateSearchResults();
  });
}

/**
 * Restores the last set of bookmarks that was deleted.
 */
function undoDelete() {
  lastDeleted.nodes.forEach(function(arr) {
    arr.forEach(restoreTree);
  });
  lastDeleted = null;
  $('undo-delete-command').canExecuteChange();
  $('undo-delete-from-folders-menu-command').canExecuteChange();

  // Only a single level of undo is supported, so disable global undo now.
  performGlobalUndo = null;
}

/**
 * Computes folder for "Add Page" and "Add Folder".
 * @return {string} The id of folder node where we'll create new page/folder.
 */
function computeParentFolderForNewItem() {
  if (document.activeElement == bmm.tree)
    return bmm.list.parentId;
  var selectedItem = bmm.list.selectedItem;
  return selectedItem && bmm.isFolder(selectedItem) ?
      selectedItem.id : bmm.list.parentId;
}

/**
 * Callback for rename folder and edit command. This starts editing for
 * the passed in target, or the selected item.
 * @param {EventTarget=} opt_target The target to start editing. If absent or
 *     null, the selected item will be edited instead.
 */
function editItem(opt_target) {
  if ((opt_target || document.activeElement) == bmm.tree) {
    bmm.tree.selectedItem.editing = true;
  } else {
    var li = bmm.list.getListItem(bmm.list.selectedItem);
    if (li)
      li.editing = true;
  }
}

/**
 * Callback for the new folder command. This creates a new folder and starts
 * a rename of it.
 * @param {EventTarget=} opt_target The target to create a new folder in.
 */
function newFolder(opt_target) {
  performGlobalUndo = null;  // This can't be undone, so disable global undo.

  var parentId = computeParentFolderForNewItem();
  var selectedItems = bmm.list.selectedItems;
  var newIndex;
  // Callback is called after tree and list data model updated.
  function createFolder(callback) {
    if (selectedItems.length == 1 && document.activeElement != bmm.tree &&
        !bmm.isFolder(selectedItems[0]) && selectedItems[0].id != 'new') {
      newIndex = bmm.list.dataModel.indexOf(selectedItems[0]) + 1;
    }
    chrome.bookmarks.create({
      title: loadTimeData.getString('new_folder_name'),
      parentId: parentId,
      index: newIndex
    }, callback);
  }

  if ((opt_target || document.activeElement) == bmm.tree) {
    createFolder(function(newNode) {
      navigateTo(newNode.id, function() {
        bmm.treeLookup[newNode.id].editing = true;
      });
    });
    return;
  }

  function editNewFolderInList() {
    createFolder(function(newNode) {
      var index = newNode.index;
      var sm = bmm.list.selectionModel;
      sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
      scrollIntoViewAndMakeEditable(index);
    });
  }

  navigateTo(parentId, editNewFolderInList);
}

/**
 * Scrolls the list item into view and makes it editable.
 * @param {number} index The index of the item to make editable.
 */
function scrollIntoViewAndMakeEditable(index) {
  bmm.list.scrollIndexIntoView(index);
  // onscroll is now dispatched asynchronously so we have to postpone
  // the rest.
  setTimeout(function() {
    var item = bmm.list.getListItemByIndex(index);
    if (item)
      item.editing = true;
  }, 0);
}

/**
 * Adds a page to the current folder. This is called by the
 * add-new-bookmark-command handler.
 */
function addPage() {
  var parentId = computeParentFolderForNewItem();
  var selectedItems = bmm.list.selectedItems;
  var newIndex;
  function editNewBookmark() {
    if (selectedItems.length == 1 && document.activeElement != bmm.tree &&
        !bmm.isFolder(selectedItems[0])) {
      newIndex = bmm.list.dataModel.indexOf(selectedItems[0]) + 1;
    }

    var fakeNode = {
      title: '',
      url: '',
      parentId: parentId,
      index: newIndex,
      id: 'new'
    };
    var dataModel = bmm.list.dataModel;
    var index = dataModel.length;
    if (newIndex != undefined)
      index = newIndex;
    dataModel.splice(index, 0, fakeNode);
    var sm = bmm.list.selectionModel;
    sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
    scrollIntoViewAndMakeEditable(index);
  };

  navigateTo(parentId, editNewBookmark);
}

/**
 * This function is used to select items after a user action such as paste, drop
 * add page etc.
 * @param {BookmarkList|BookmarkTree} target The target of the user action.
 * @param {string=} opt_selectedTreeId If provided, then select that tree id.
 */
function selectItemsAfterUserAction(target, opt_selectedTreeId) {
  // We get one onCreated event per item so we delay the handling until we get
  // no more events coming.

  var ids = [];
  var timer;

  function handle(id, bookmarkNode) {
    clearTimeout(timer);
    if (opt_selectedTreeId || bmm.list.parentId == bookmarkNode.parentId)
      ids.push(id);
    timer = setTimeout(handleTimeout, 50);
  }

  function handleTimeout() {
    chrome.bookmarks.onCreated.removeListener(handle);
    chrome.bookmarks.onMoved.removeListener(handle);

    if (opt_selectedTreeId && ids.indexOf(opt_selectedTreeId) != -1) {
      var index = ids.indexOf(opt_selectedTreeId);
      if (index != -1 && opt_selectedTreeId in bmm.treeLookup) {
        bmm.tree.selectedItem = bmm.treeLookup[opt_selectedTreeId];
      }
    } else if (target == bmm.list) {
      var dataModel = bmm.list.dataModel;
      var firstIndex = dataModel.findIndexById(ids[0]);
      var lastIndex = dataModel.findIndexById(ids[ids.length - 1]);
      if (firstIndex != -1 && lastIndex != -1) {
        var selectionModel = bmm.list.selectionModel;
        selectionModel.selectedIndex = -1;
        selectionModel.selectRange(firstIndex, lastIndex);
        selectionModel.anchorIndex = selectionModel.leadIndex = lastIndex;
        bmm.list.focus();
      }
    }

    bmm.list.endBatchUpdates();
  }

  bmm.list.startBatchUpdates();

  chrome.bookmarks.onCreated.addListener(handle);
  chrome.bookmarks.onMoved.addListener(handle);
  timer = setTimeout(handleTimeout, 300);
}

/**
 * Record user action.
 * @param {string} name An user action name.
 */
function recordUserAction(name) {
  chrome.metricsPrivate.recordUserAction('BookmarkManager_Command_' + name);
}

/**
 * The currently selected bookmark, based on where the user is clicking.
 * @return {string} The ID of the currently selected bookmark (could be from
 *     tree view or list view).
 */
function getSelectedId() {
  if (document.activeElement == bmm.tree)
    return bmm.tree.selectedItem.bookmarkId;
  var selectedItem = bmm.list.selectedItem;
  return selectedItem && bmm.isFolder(selectedItem) ?
      selectedItem.id : bmm.tree.selectedItem.bookmarkId;
}

/**
 * Pastes the copied/cutted bookmark into the right location depending whether
 * if it was called from Organize Menu or from Context Menu.
 * @param {string} id The id of the element being pasted from.
 */
function pasteBookmark(id) {
  recordUserAction('Paste');
  selectItemsAfterUserAction(/** @type {BookmarkList} */(bmm.list));
  chrome.bookmarkManagerPrivate.paste(id, getSelectedBookmarkIds());
}

/**
 * Returns true if child is contained in another selected folder.
 * Traces parent nodes up the tree until a selected ancestor or root is found.
 */
function hasSelectedAncestor(parentNode) {
  function contains(arr, item) {
    for (var i = 0; i < arr.length; i++)
        if (arr[i] === item)
          return true;
    return false;
  }

  // Don't search top level, cannot select permanent nodes in search.
  if (parentNode == null || parentNode.id <= 2)
    return false;

  // Found selected ancestor.
  if (contains(getSelectedBookmarkNodes(), parentNode))
    return true;

  // Keep digging.
  return hasSelectedAncestor(
      bmm.tree.getBookmarkNodeById(parentNode.parentId));
}

/**
 * @param {EventTarget=} opt_target A target to get bookmark IDs from.
 * @return {Array<string>} An array of bookmarks IDs.
 */
function getFilteredSelectedBookmarkIds(opt_target) {
  // Remove duplicates from filteredIds and return.
  var filteredIds = [];
  // Selected nodes to iterate through for matches.
  var nodes = getSelectedBookmarkNodes(opt_target);

  for (var i = 0; i < nodes.length; i++)
    if (!hasSelectedAncestor(bmm.tree.getBookmarkNodeById(nodes[i].parentId)))
      filteredIds.splice(0, 0, nodes[i].id);

  return filteredIds;
}

/**
 * Handler for the command event. This is used for context menu of list/tree
 * and organized menu.
 * @param {!Event} e The event object.
 */
function handleCommand(e) {
  var command = e.command;
  var target = assertInstanceof(e.target, HTMLElement);
  switch (command.id) {
    case 'import-menu-command':
      recordUserAction('Import');
      chrome.bookmarks.import();
      break;

    case 'export-menu-command':
      recordUserAction('Export');
      chrome.bookmarks.export();
      break;

    case 'undo-command':
      if (performGlobalUndo) {
        recordUserAction('UndoGlobal');
        performGlobalUndo();
      } else {
        recordUserAction('UndoNone');
      }
      break;

    case 'show-in-folder-command':
      recordUserAction('ShowInFolder');
      showInFolder();
      break;

    case 'open-in-new-tab-command':
    case 'open-in-background-tab-command':
      recordUserAction('OpenInNewTab');
      openBookmarks(LinkKind.BACKGROUND_TAB, target);
      break;

    case 'open-in-new-window-command':
      recordUserAction('OpenInNewWindow');
      openBookmarks(LinkKind.WINDOW, target);
      break;

    case 'open-incognito-window-command':
      recordUserAction('OpenIncognito');
      openBookmarks(LinkKind.INCOGNITO, target);
      break;

    case 'delete-from-folders-menu-command':
      target = bmm.tree;
    case 'delete-command':
      recordUserAction('Delete');
      deleteBookmarks(target);
      break;

    case 'copy-from-folders-menu-command':
      target = bmm.tree;
    case 'copy-command':
      recordUserAction('Copy');
      chrome.bookmarkManagerPrivate.copy(getSelectedBookmarkIds(target),
                                         updatePasteCommand);
      break;

    case 'cut-from-folders-menu-command':
      target = bmm.tree;
    case 'cut-command':
      recordUserAction('Cut');
      chrome.bookmarkManagerPrivate.cut(getSelectedBookmarkIds(target),
                                        function() {
                                          updatePasteCommand();
                                          updateSearchResults();
                                        });
      break;

    case 'paste-from-organize-menu-command':
      pasteBookmark(bmm.list.parentId);
      break;

    case 'paste-from-folders-menu-command':
      pasteBookmark(bmm.tree.selectedItem.bookmarkId);
      break;

    case 'paste-from-context-menu-command':
      pasteBookmark(getSelectedId());
      break;

    case 'sort-command':
      recordUserAction('Sort');
      chrome.bookmarkManagerPrivate.sortChildren(bmm.list.parentId);
      break;


    case 'rename-folder-from-folders-menu-command':
      target = bmm.tree;
    case 'rename-folder-command':
      editItem(target);
      break;

    case 'edit-command':
      recordUserAction('Edit');
      editItem();
      break;

    case 'new-folder-from-folders-menu-command':
      target = bmm.tree;
    case 'new-folder-command':
      recordUserAction('NewFolder');
      newFolder(target);
      break;

    case 'add-new-bookmark-command':
      recordUserAction('AddPage');
      addPage();
      break;

    case 'open-in-same-window-command':
      recordUserAction('OpenInSame');
      openItem();
      break;

    case 'undo-delete-command':
    case 'undo-delete-from-folders-menu-command':
      recordUserAction('UndoDelete');
      undoDelete();
      break;
  }
}

// Execute the copy, cut and paste commands when those events are dispatched by
// the browser. This allows us to rely on the browser to handle the keyboard
// shortcuts for these commands.
function installEventHandlerForCommand(eventName, commandId) {
  function handle(e) {
    if (document.activeElement != bmm.list &&
        document.activeElement != bmm.tree)
      return;
    var command = $(commandId);
    if (!command.disabled) {
      command.execute();
      if (e)
        e.preventDefault();  // Prevent the system beep.
    }
  }
  if (eventName == 'paste') {
    // Paste is a bit special since we need to do an async call to see if we
    // can paste because the paste command might not be up to date.
    document.addEventListener(eventName, function(e) {
      updatePasteCommand(handle);
    });
  } else {
    document.addEventListener(eventName, handle);
  }
}

function initializeSplitter() {
  var splitter = document.querySelector('.main > .splitter');
  Splitter.decorate(splitter);

  var splitterStyle = splitter.previousElementSibling.style;

  // The splitter persists the size of the left component in the local store.
  if ('treeWidth' in window.localStorage)
    splitterStyle.width = window.localStorage['treeWidth'];

  splitter.addEventListener('resize', function(e) {
    window.localStorage['treeWidth'] = splitterStyle.width;
  });
}

function initializeBookmarkManager() {
  // Sometimes the extension API is not initialized.
  if (!chrome.bookmarks)
    console.error('Bookmarks extension API is not available');

  chrome.bookmarkManagerPrivate.getStrings(continueInitializeBookmarkManager);
}

function continueInitializeBookmarkManager(localizedStrings) {
  loadLocalizedStrings(localizedStrings);

  bmm.treeLookup[searchTreeItem.bookmarkId] = searchTreeItem;

  cr.ui.decorate('cr-menu', Menu);
  cr.ui.decorate('button[menu]', MenuButton);
  cr.ui.decorate('command', Command);
  BookmarkList.decorate($('list'));
  BookmarkTree.decorate($('tree'));

  bmm.list.addEventListener('canceledit', handleCancelEdit);
  bmm.list.addEventListener('canExecute', handleCanExecuteForList);
  bmm.list.addEventListener('change', updateAllCommands);
  bmm.list.addEventListener('contextmenu', updateEditingCommands);
  bmm.list.addEventListener('dblclick', handleDoubleClickForList);
  bmm.list.addEventListener('edit', handleEdit);
  bmm.list.addEventListener('rename', handleRename);
  bmm.list.addEventListener('urlClicked', handleUrlClickedForList);

  bmm.tree.addEventListener('canExecute', handleCanExecuteForTree);
  bmm.tree.addEventListener('change', handleChangeForTree);
  bmm.tree.addEventListener('contextmenu', updateEditingCommands);
  bmm.tree.addEventListener('rename', handleRename);
  bmm.tree.addEventListener('load', handleLoadForTree);

  cr.ui.contextMenuHandler.addContextMenuProperty(
      /** @type {!Element} */(bmm.tree));
  bmm.list.contextMenu = $('context-menu');
  bmm.tree.contextMenu = $('context-menu');

  // We listen to hashchange so that we can update the currently shown folder
  // when // the user goes back and forward in the history.
  window.addEventListener('hashchange', processHash);

  document.querySelector('header form').onsubmit =
      /** @type {function(Event=)} */(function(e) {
    setSearch($('term').value);
    e.preventDefault();
  });

  $('term').addEventListener('search', handleSearch);
  $('term').addEventListener('canExecute', handleCanExecuteForSearchBox);

  $('folders-button').addEventListener('click', handleMenuButtonClicked);
  $('organize-button').addEventListener('click', handleMenuButtonClicked);

  document.addEventListener('canExecute', handleCanExecuteForDocument);
  document.addEventListener('command', handleCommand);

  // Listen to copy, cut and paste events and execute the associated commands.
  installEventHandlerForCommand('copy', 'copy-command');
  installEventHandlerForCommand('cut', 'cut-command');
  installEventHandlerForCommand('paste', 'paste-from-organize-menu-command');

  // Install shortcuts
  for (var name in commandShortcutMap) {
    $(name + '-command').shortcut = commandShortcutMap[name];
  }

  // Disable almost all commands at startup.
  var commands = document.querySelectorAll('command');
  for (var i = 0, command; command = commands[i]; ++i) {
    if (command.id != 'import-menu-command' &&
        command.id != 'export-menu-command') {
      command.disabled = true;
    }
  }

  chrome.bookmarkManagerPrivate.canEdit(function(result) {
    canEdit = result;
  });

  chrome.systemPrivate.getIncognitoModeAvailability(function(result) {
    // TODO(rustema): propagate policy value to the bookmark manager when it
    // changes.
    incognitoModeAvailability = result;
  });

  chrome.bookmarkManagerPrivate.canOpenNewWindows(function(result) {
    canOpenNewWindows = result;
  });

  cr.ui.FocusOutlineManager.forDocument(document);
  initializeSplitter();
  bmm.addBookmarkModelListeners();
  dnd.init(selectItemsAfterUserAction);
  bmm.tree.reload();
}

initializeBookmarkManager();
})();
