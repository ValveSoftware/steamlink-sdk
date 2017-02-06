// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the automation API.
var AutomationNode = require('automationNode').AutomationNode;
var AutomationRootNode = require('automationNode').AutomationRootNode;
var automation = require('binding').Binding.create('automation');
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var eventBindings = require('event_bindings');
var Event = eventBindings.Event;
var exceptionHandler = require('uncaught_exception_handler');
var forEach = require('utils').forEach;
var lastError = require('lastError');
var logging = requireNative('logging');
var nativeAutomationInternal = requireNative('automationInternal');
var GetRoutingID = nativeAutomationInternal.GetRoutingID;
var GetSchemaAdditions = nativeAutomationInternal.GetSchemaAdditions;
var DestroyAccessibilityTree =
    nativeAutomationInternal.DestroyAccessibilityTree;
var GetIntAttribute = nativeAutomationInternal.GetIntAttribute;
var StartCachingAccessibilityTrees =
    nativeAutomationInternal.StartCachingAccessibilityTrees;
var AddTreeChangeObserver = nativeAutomationInternal.AddTreeChangeObserver;
var RemoveTreeChangeObserver =
    nativeAutomationInternal.RemoveTreeChangeObserver;
var GetFocus = nativeAutomationInternal.GetFocus;
var schema = GetSchemaAdditions();

/**
 * A namespace to export utility functions to other files in automation.
 */
window.automationUtil = function() {};

// TODO(aboxhall): Look into using WeakMap
var idToCallback = {};

var DESKTOP_TREE_ID = 0;

automationUtil.storeTreeCallback = function(id, callback) {
  if (!callback)
    return;

  var targetTree = AutomationRootNode.get(id);
  if (!targetTree) {
    // If we haven't cached the tree, hold the callback until the tree is
    // populated by the initial onAccessibilityEvent call.
    if (id in idToCallback)
      idToCallback[id].push(callback);
    else
      idToCallback[id] = [callback];
  } else {
    callback(targetTree);
  }
};

/**
 * Global list of tree change observers.
 * @type {Object<number, TreeChangeObserver>}
 */
automationUtil.treeChangeObserverMap = {};

/**
 * The id of the next tree change observer.
 * @type {number}
 */
automationUtil.nextTreeChangeObserverId = 1;

/**
 * @type {AutomationNode} The current focused node. This is only updated
 *   when calling automationUtil.updateFocusedNode.
 */
automationUtil.focusedNode = null;

/**
 * Update automationUtil.focusedNode to be the node that currently has focus.
 */
automationUtil.updateFocusedNode = function() {
  automationUtil.focusedNode = null;
  var focusedNodeInfo = GetFocus(DESKTOP_TREE_ID);
  if (!focusedNodeInfo)
    return;
  var tree = AutomationRootNode.getOrCreate(focusedNodeInfo.treeId);
  if (tree) {
    automationUtil.focusedNode =
        privates(tree).impl.get(focusedNodeInfo.nodeId);
  }
};

automation.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // TODO(aboxhall, dtseng): Make this return the speced AutomationRootNode obj.
  apiFunctions.setHandleRequest('getTree', function getTree(tabID, callback) {
    var routingID = GetRoutingID();
    StartCachingAccessibilityTrees();

    // enableTab() ensures the renderer for the active or specified tab has
    // accessibility enabled, and fetches its ax tree id to use as
    // a key in the idToAutomationRootNode map. The callback to
    // enableTab is bound to the callback passed in to getTree(), so that once
    // the tree is available (either due to having been cached earlier, or after
    // an accessibility event occurs which causes the tree to be populated), the
    // callback can be called.
    var params = { routingID: routingID, tabID: tabID };
    automationInternal.enableTab(params,
        function onEnable(id) {
          if (lastError.hasError(chrome)) {
            callback();
            return;
          }
          automationUtil.storeTreeCallback(id, callback);
        });
  });

  var desktopTree = null;
  apiFunctions.setHandleRequest('getDesktop', function(callback) {
    StartCachingAccessibilityTrees();
    desktopTree = AutomationRootNode.get(DESKTOP_TREE_ID);
    if (!desktopTree) {
      if (DESKTOP_TREE_ID in idToCallback)
        idToCallback[DESKTOP_TREE_ID].push(callback);
      else
        idToCallback[DESKTOP_TREE_ID] = [callback];

      var routingID = GetRoutingID();

      // TODO(dtseng): Disable desktop tree once desktop object goes out of
      // scope.
      automationInternal.enableDesktop(routingID, function() {
        if (lastError.hasError(chrome)) {
          AutomationRootNode.destroy(DESKTOP_TREE_ID);
          callback();
          return;
        }
      });
    } else {
      callback(desktopTree);
    }
  });

  apiFunctions.setHandleRequest('getFocus', function(callback) {
    automationUtil.updateFocusedNode();
    callback(automationUtil.focusedNode);
  });

  function removeTreeChangeObserver(observer) {
    for (var id in automationUtil.treeChangeObserverMap) {
      if (automationUtil.treeChangeObserverMap[id] == observer) {
        RemoveTreeChangeObserver(id);
        delete automationUtil.treeChangeObserverMap[id];
        return;
      }
    }
  }
  apiFunctions.setHandleRequest('removeTreeChangeObserver', function(observer) {
    removeTreeChangeObserver(observer);
  });

  function addTreeChangeObserver(filter, observer) {
    removeTreeChangeObserver(observer);
    var id = automationUtil.nextTreeChangeObserverId++;
    AddTreeChangeObserver(id, filter);
    automationUtil.treeChangeObserverMap[id] = observer;
  }
  apiFunctions.setHandleRequest('addTreeChangeObserver',
      function(filter, observer) {
    addTreeChangeObserver(filter, observer);
  });

  apiFunctions.setHandleRequest('setDocumentSelection', function(params) {
    var anchorNodeImpl = privates(params.anchorObject).impl;
    var focusNodeImpl = privates(params.focusObject).impl;
    if (anchorNodeImpl.treeID !== focusNodeImpl.treeID)
      throw new Error('Selection anchor and focus must be in the same tree.');
    if (anchorNodeImpl.treeID === DESKTOP_TREE_ID) {
      throw new Error('Use AutomationNode.setSelection to set the selection ' +
          'in the desktop tree.');
    }
    automationInternal.performAction({ treeID: anchorNodeImpl.treeID,
                                       automationNodeID: anchorNodeImpl.id,
                                       actionType: 'setSelection'},
                                     { focusNodeID: focusNodeImpl.id,
                                       anchorOffset: params.anchorOffset,
                                       focusOffset: params.focusOffset });
  });

});

automationInternal.onChildTreeID.addListener(function(treeID,
                                                      nodeID) {
  var tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree)
    return;

  var node = privates(tree).impl.get(nodeID);
  if (!node)
    return;

  // A WebView in the desktop tree has a different AX tree as its child.
  // When we encounter a WebView with a child AX tree id that we don't
  // currently have cached, explicitly request that AX tree from the
  // browser process and set up a callback when it loads to attach that
  // tree as a child of this node and fire appropriate events.
  var childTreeID = GetIntAttribute(treeID, nodeID, 'childTreeId');
  if (!childTreeID)
    return;

  var subroot = AutomationRootNode.get(childTreeID);
  if (!subroot) {
    automationUtil.storeTreeCallback(childTreeID, function(root) {
      // Return early if the root has already been attached.
      if (root.parent)
        return;

      privates(root).impl.setHostNode(node);

      if (root.docLoaded)
        privates(root).impl.dispatchEvent(schema.EventType.loadComplete);

      privates(node).impl.dispatchEvent(schema.EventType.childrenChanged);
    });

    automationInternal.enableFrame(childTreeID);
  } else {
    privates(subroot).impl.setHostNode(node);
  }
});

automationInternal.onTreeChange.addListener(function(observerID,
                                                     treeID,
                                                     nodeID,
                                                     changeType) {
  var tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree)
    return;

  var node = privates(tree).impl.get(nodeID);
  if (!node)
    return;

  var observer = automationUtil.treeChangeObserverMap[observerID];
  if (!observer)
    return;

  try {
    observer({target: node, type: changeType});
  } catch (e) {
    exceptionHandler.handle('Error in tree change observer for ' +
        treeChange.type, e);
  }
});

automationInternal.onNodesRemoved.addListener(function(treeID, nodeIDs) {
  var tree = AutomationRootNode.getOrCreate(treeID);
  if (!tree)
    return;

  for (var i = 0; i < nodeIDs.length; i++) {
    privates(tree).impl.remove(nodeIDs[i]);
  }
});

/**
 * Dispatch accessibility events fired on individual nodes to its
 * corresponding AutomationNode. Handle focus events specially
 * (see below).
 */
automationInternal.onAccessibilityEvent.addListener(function(eventParams) {
  var id = eventParams.treeID;
  var targetTree = AutomationRootNode.getOrCreate(id);

  // When we get a focus event, ignore the actual event target, and instead
  // check what node has focus globally. If that represents a focus change,
  // fire a focus event on the correct target.
  if (eventParams.eventType == schema.EventType.focus) {
    var previousFocusedNode = automationUtil.focusedNode;
    automationUtil.updateFocusedNode();
    if (automationUtil.focusedNode &&
        automationUtil.focusedNode == previousFocusedNode) {
      return;
    }

    if (automationUtil.focusedNode) {
      targetTree = automationUtil.focusedNode.root;
      eventParams.treeID = privates(targetTree).impl.treeID;
      eventParams.targetID = privates(automationUtil.focusedNode).impl.id;
    }
  }

  if (!privates(targetTree).impl.onAccessibilityEvent(eventParams))
    return;

  // If we're not waiting on a callback to getTree(), we can early out here.
  if (!(id in idToCallback))
    return;

  // We usually get a 'placeholder' tree first, which doesn't have any url
  // attribute or child nodes. If we've got that, wait for the full tree before
  // calling the callback.
  // TODO(dmazzoni): Don't send down placeholder (crbug.com/397553)
  if (id != DESKTOP_TREE_ID && !targetTree.url &&
      targetTree.children.length == 0)
    return;

  // If the tree wasn't available when getTree() was called, the callback will
  // have been cached in idToCallback, so call and delete it now that we
  // have the complete tree.
  for (var i = 0; i < idToCallback[id].length; i++) {
    var callback = idToCallback[id][i];
    callback(targetTree);
  }
  delete idToCallback[id];
});

automationInternal.onAccessibilityTreeDestroyed.addListener(function(id) {
  // Destroy the AutomationRootNode.
  var targetTree = AutomationRootNode.get(id);
  if (targetTree) {
    privates(targetTree).impl.destroy();
    AutomationRootNode.destroy(id);
  } else {
    logging.WARNING('no targetTree to destroy');
  }

  // Destroy the native cache of the accessibility tree.
  DestroyAccessibilityTree(id);
});

var binding = automation.generate();
// Add additional accessibility bindings not specified in the automation IDL.
// Accessibility and automation share some APIs (see
// ui/accessibility/ax_enums.idl).
forEach(schema, function(k, v) {
  binding[k] = v;
});

exports.$set('binding', binding);
