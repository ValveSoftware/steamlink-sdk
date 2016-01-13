// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = require('automationEvent').AutomationEvent;
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var utils = require('utils');
var IsInteractPermitted =
    requireNative('automationInternal').IsInteractPermitted;

/**
 * A single node in the Automation tree.
 * @param {AutomationRootNodeImpl} root The root of the tree.
 * @constructor
 */
function AutomationNodeImpl(root) {
  this.rootImpl = root;
  this.childIds = [];
  this.attributes = {};
  this.listeners = {};
  this.location = { left: 0, top: 0, width: 0, height: 0 };
}

AutomationNodeImpl.prototype = {
  id: -1,
  role: '',
  state: { busy: true },
  isRootNode: false,

  get root() {
    return this.rootImpl.wrapper;
  },

  parent: function() {
    return this.rootImpl.get(this.parentID);
  },

  firstChild: function() {
    var node = this.rootImpl.get(this.childIds[0]);
    return node;
  },

  lastChild: function() {
    var childIds = this.childIds;
    var node = this.rootImpl.get(childIds[childIds.length - 1]);
    return node;
  },

  children: function() {
    var children = [];
    for (var i = 0, childID; childID = this.childIds[i]; i++)
      children.push(this.rootImpl.get(childID));
    return children;
  },

  previousSibling: function() {
    var parent = this.parent();
    if (parent && this.indexInParent > 0)
      return parent.children()[this.indexInParent - 1];
    return undefined;
  },

  nextSibling: function() {
    var parent = this.parent();
    if (parent && this.indexInParent < parent.children().length)
      return parent.children()[this.indexInParent + 1];
    return undefined;
  },

  doDefault: function() {
    this.performAction_('doDefault');
  },

  focus: function(opt_callback) {
    this.performAction_('focus');
  },

  makeVisible: function(opt_callback) {
    this.performAction_('makeVisible');
  },

  setSelection: function(startIndex, endIndex, opt_callback) {
    this.performAction_('setSelection',
                        { startIndex: startIndex,
                          endIndex: endIndex });
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    this.listeners[eventType].push({callback: callback, capture: !!capture});
  },

  // TODO(dtseng/aboxhall): Check this impl against spec.
  removeEventListener: function(eventType, callback) {
    if (this.listeners[eventType]) {
      var listeners = this.listeners[eventType];
      for (var i = 0; i < listeners.length; i++) {
        if (callback === listeners[i].callback)
          listeners.splice(i, 1);
      }
    }
  },

  dispatchEvent: function(eventType) {
    var path = [];
    var parent = this.parent();
    while (parent) {
      path.push(parent);
      // TODO(aboxhall/dtseng): handle unloaded parent node
      parent = parent.parent();
    }
    var event = new AutomationEvent(eventType, this.wrapper);

    // Dispatch the event through the propagation path in three phases:
    // - capturing: starting from the root and going down to the target's parent
    // - targeting: dispatching the event on the target itself
    // - bubbling: starting from the target's parent, going back up to the root.
    // At any stage, a listener may call stopPropagation() on the event, which
    // will immediately stop event propagation through this path.
    if (this.dispatchEventAtCapturing_(event, path)) {
      if (this.dispatchEventAtTargeting_(event, path))
        this.dispatchEventAtBubbling_(event, path);
    }
  },

  toString: function() {
    return 'node id=' + this.id +
        ' role=' + this.role +
        ' state=' + JSON.stringify(this.state) +
        ' childIds=' + JSON.stringify(this.childIds) +
        ' attributes=' + JSON.stringify(this.attributes);
  },

  dispatchEventAtCapturing_: function(event, path) {
    privates(event).impl.eventPhase = Event.CAPTURING_PHASE;
    for (var i = path.length - 1; i >= 0; i--) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  dispatchEventAtTargeting_: function(event) {
    privates(event).impl.eventPhase = Event.AT_TARGET;
    this.fireEventListeners_(this.wrapper, event);
    return !privates(event).impl.propagationStopped;
  },

  dispatchEventAtBubbling_: function(event, path) {
    privates(event).impl.eventPhase = Event.BUBBLING_PHASE;
    for (var i = 0; i < path.length; i++) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  fireEventListeners_: function(node, event) {
    var nodeImpl = privates(node).impl;
    var listeners = nodeImpl.listeners[event.type];
    if (!listeners)
      return;
    var eventPhase = event.eventPhase;
    for (var i = 0; i < listeners.length; i++) {
      if (eventPhase == Event.CAPTURING_PHASE && !listeners[i].capture)
        continue;
      if (eventPhase == Event.BUBBLING_PHASE && listeners[i].capture)
        continue;

      try {
        listeners[i].callback(event);
      } catch (e) {
        console.error('Error in event handler for ' + event.type +
                      'during phase ' + eventPhase + ': ' +
                      e.message + '\nStack trace: ' + e.stack);
      }
    }
  },

  performAction_: function(actionType, opt_args) {
    // Not yet initialized.
    if (this.rootImpl.processID === undefined ||
        this.rootImpl.routingID === undefined ||
        this.wrapper.id === undefined) {
      return;
    }

    // Check permissions.
    if (!IsInteractPermitted()) {
      throw new Error(actionType + ' requires {"desktop": true} or' +
          ' {"interact": true} in the "automation" manifest key.');
    }

    automationInternal.performAction({ processID: this.rootImpl.processID,
                                       routingID: this.rootImpl.routingID,
                                       automationNodeID: this.wrapper.id,
                                       actionType: actionType },
                                     opt_args || {});
  }
};

// Maps an attribute to its default value in an invalidated node.
// These attributes are taken directly from the Automation idl.
var AutomationAttributeDefaults = {
  'id': -1,
  'role': '',
  'state': {},
  'location': { left: 0, top: 0, width: 0, height: 0 }
};


var AutomationAttributeTypes = [
  'boolAttributes',
  'floatAttributes',
  'htmlAttributes',
  'intAttributes',
  'intlistAttributes',
  'stringAttributes'
];


/**
 * AutomationRootNode.
 *
 * An AutomationRootNode is the javascript end of an AXTree living in the
 * browser. AutomationRootNode handles unserializing incremental updates from
 * the source AXTree. Each update contains node data that form a complete tree
 * after applying the update.
 *
 * A brief note about ids used through this class. The source AXTree assigns
 * unique ids per node and we use these ids to build a hash to the actual
 * AutomationNode object.
 * Thus, tree traversals amount to a lookup in our hash.
 *
 * The tree itself is identified by the process id and routing id of the
 * renderer widget host.
 * @constructor
 */
function AutomationRootNodeImpl(processID, routingID) {
  AutomationNodeImpl.call(this, this);
  this.processID = processID;
  this.routingID = routingID;
  this.axNodeDataCache_ = {};
}

AutomationRootNodeImpl.prototype = {
  __proto__: AutomationNodeImpl.prototype,

  isRootNode: true,

  get: function(id) {
    return this.axNodeDataCache_[id];
  },

  invalidate: function(node) {
    if (!node)
      return;

    var children = node.children();

    for (var i = 0, child; child = children[i]; i++)
      this.invalidate(child);

    // Retrieve the internal AutomationNodeImpl instance for this node.
    // This object is not accessible outside of bindings code, but we can access
    // it here.
    var nodeImpl = privates(node).impl;
    var id = node.id;
    for (var key in AutomationAttributeDefaults) {
      nodeImpl[key] = AutomationAttributeDefaults[key];
    }
    nodeImpl.loaded = false;
    nodeImpl.id = id;
    this.axNodeDataCache_[id] = undefined;
  },

  update: function(data) {
    var didUpdateRoot = false;

    if (data.nodes.length == 0)
      return false;

    for (var i = 0; i < data.nodes.length; i++) {
      var nodeData = data.nodes[i];
      var node = this.axNodeDataCache_[nodeData.id];
      if (!node) {
        if (nodeData.role == 'rootWebArea' || nodeData.role == 'desktop') {
          // |this| is an AutomationRootNodeImpl; retrieve the
          // AutomationRootNode instance instead.
          node = this.wrapper;
          didUpdateRoot = true;
        } else {
          node = new AutomationNode(this);
        }
      }
      var nodeImpl = privates(node).impl;

      // Update children.
      var oldChildIDs = nodeImpl.childIds;
      var newChildIDs = nodeData.childIds || [];
      var newChildIDsHash = {};

      for (var j = 0, newId; newId = newChildIDs[j]; j++) {
        // Hash the new child ids for faster lookup.
        newChildIDsHash[newId] = newId;

        // We need to update all new children's parent id regardless.
        var childNode = this.get(newId);
        if (!childNode) {
          childNode = new AutomationNode(this);
          this.axNodeDataCache_[newId] = childNode;
          privates(childNode).impl.id = newId;
        }
        privates(childNode).impl.indexInParent = j;
        privates(childNode).impl.parentID = nodeData.id;
      }

      for (var k = 0, oldId; oldId = oldChildIDs[k]; k++) {
        // However, we must invalidate all old child ids that are no longer
        // children.
        if (!newChildIDsHash[oldId]) {
          this.invalidate(this.get(oldId));
        }
      }

      for (var key in AutomationAttributeDefaults) {
        if (key in nodeData)
          nodeImpl[key] = nodeData[key];
        else
          nodeImpl[key] = AutomationAttributeDefaults[key];
      }
      for (var attributeTypeIndex = 0;
           attributeTypeIndex < AutomationAttributeTypes.length;
           attributeTypeIndex++) {
        var attributeType = AutomationAttributeTypes[attributeTypeIndex];
        for (var attributeID in nodeData[attributeType]) {
          nodeImpl.attributes[attributeID] =
              nodeData[attributeType][attributeID];
        }
      }
      nodeImpl.childIds = newChildIDs;
      nodeImpl.loaded = true;
      this.axNodeDataCache_[node.id] = node;
    }
    var node = this.get(data.targetID);
    if (node)
      nodeImpl.dispatchEvent(data.eventType);
    return true;
  },

  toString: function() {
    function toStringInternal(node, indent) {
      if (!node)
        return '';
      var output =
        new Array(indent).join(' ') + privates(node).impl.toString() + '\n';
      indent += 2;
      for (var i = 0; i < node.children().length; i++)
        output += toStringInternal(node.children()[i], indent);
      return output;
    }
    return toStringInternal(this, 0);
  }
};


var AutomationNode = utils.expose('AutomationNode',
                                  AutomationNodeImpl,
                                  { functions: ['parent',
                                                'firstChild',
                                                'lastChild',
                                                'children',
                                                'previousSibling',
                                                'nextSibling',
                                                'doDefault',
                                                'focus',
                                                'makeVisible',
                                                'setSelection',
                                                'addEventListener',
                                                'removeEventListener'],
                                    readonly: ['isRootNode',
                                               'id',
                                               'role',
                                               'state',
                                               'location',
                                               'attributes'] });

var AutomationRootNode = utils.expose('AutomationRootNode',
                                      AutomationRootNodeImpl,
                                      { superclass: AutomationNode,
                                        functions: ['load'],
                                        readonly: ['loaded'] });

exports.AutomationNode = AutomationNode;
exports.AutomationRootNode = AutomationRootNode;
