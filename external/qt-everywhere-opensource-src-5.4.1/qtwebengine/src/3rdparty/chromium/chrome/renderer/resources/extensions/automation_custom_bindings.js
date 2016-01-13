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
var forEach = require('utils').forEach;
var lastError = require('lastError');
var schema =
    requireNative('automationInternal').GetSchemaAdditions();

// TODO(aboxhall): Look into using WeakMap
var idToAutomationRootNode = {};
var idToCallback = {};

// TODO(dtseng): Move out to automation/automation_util.js or as a static member
// of AutomationRootNode to keep this file clean.
/*
 * Creates an id associated with a particular AutomationRootNode based upon a
 * renderer/renderer host pair's process and routing id.
 */
var createAutomationRootNodeID = function(pid, rid) {
  return pid + '_' + rid;
};

var DESKTOP_TREE_ID = createAutomationRootNodeID(0, 0);

automation.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // TODO(aboxhall, dtseng): Make this return the speced AutomationRootNode obj.
  apiFunctions.setHandleRequest('getTree', function getTree(tabId, callback) {
    // enableTab() ensures the renderer for the active or specified tab has
    // accessibility enabled, and fetches its process and routing ids to use as
    // a key in the idToAutomationRootNode map. The callback to enableTab is is
    // bound to the callback passed in to getTree(), so that once the tree is
    // available (either due to having been cached earlier, or after an
    // accessibility event occurs which causes the tree to be populated), the
    // callback can be called.
    automationInternal.enableTab(tabId, function onEnable(pid, rid) {
      if (lastError.hasError(chrome)) {
        callback();
        return;
      }
      var id = createAutomationRootNodeID(pid, rid);
      var targetTree = idToAutomationRootNode[id];
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
    });
  });

  var desktopTree = null;
  apiFunctions.setHandleRequest('getDesktop', function(callback) {
    desktopTree = idToAutomationRootNode[DESKTOP_TREE_ID];
    if (!desktopTree) {
      if (DESKTOP_TREE_ID in idToCallback)
        idToCallback[DESKTOP_TREE_ID].push(callback);
      else
        idToCallback[DESKTOP_TREE_ID] = [callback];

      // TODO(dtseng): Disable desktop tree once desktop object goes out of
      // scope.
      automationInternal.enableDesktop(function() {
        if (lastError.hasError(chrome)) {
          delete idToAutomationRootNode[DESKTOP_TREE_ID];
          callback();
          return;
        }
      });
    } else {
      callback(desktopTree);
    }
  });
});

// Listen to the automationInternal.onaccessibilityEvent event, which is
// essentially a proxy for the AccessibilityHostMsg_Events IPC from the
// renderer.
automationInternal.onAccessibilityEvent.addListener(function(data) {
  var pid = data.processID;
  var rid = data.routingID;
  var id = createAutomationRootNodeID(pid, rid);
  var targetTree = idToAutomationRootNode[id];
  if (!targetTree) {
    // If this is the first time we've gotten data for this tree, it will
    // contain all of the tree's data, so create a new tree which will be
    // bootstrapped from |data|.
    targetTree = new AutomationRootNode(pid, rid);
    idToAutomationRootNode[id] = targetTree;
  }
  privates(targetTree).impl.update(data);
  var eventType = data.eventType;
  if (eventType == 'loadComplete' || eventType == 'layoutComplete') {
    // If the tree wasn't available when getTree() was called, the callback will
    // have been cached in idToCallback, so call and delete it now that we
    // have the complete tree.
    if (id in idToCallback) {
      for (var i = 0; i < idToCallback[id].length; i++) {
        var callback = idToCallback[id][i];
        callback(targetTree);
      }
      delete idToCallback[id];
    }
  }
});

exports.binding = automation.generate();

// Add additional accessibility bindings not specified in the automation IDL.
// Accessibility and automation share some APIs (see
// ui/accessibility/ax_enums.idl).
forEach(schema, function(k, v) {
  exports.binding[k] = v;
});
