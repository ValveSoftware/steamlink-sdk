// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ui = (function() {
  'use strict';

  /**
   * The scene class assists in managing element and animations in the scene.
   * It allows scene update commands to be queued in batches, and manages
   * allocation of element and animation IDs.
   *
   * Examples:
   *
   * var scene = new ui.Scene();
   *
   * // Add an element.
   * var el = new api.UiElement(100, 200, 50, 50);
   * el.setSize(buttonWidth, buttonHeight);
   *
   * // Anchor it to the bottom of the content quad.
   * el.setParentId(api.getContentElementId());
   * el.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
   *
   * // Place it just below the content quad edge.
   * el.setTranslation(0, -0.2, 0.0);
   *
   * // Add it to the scene.
   * var buttonId = scene.addElement(el);
   * scene.flush();
   *
   * // Make the button twice as big.
   * var update = new api.UiElementUpdate();
   * update.setSize(bunttonWidth * 2, buttonHeight * 2);
   * scene.updateElement(buttonId, update);
   * scene.flush();
   *
   * // Animate the button size back to its original size, over 250 ms.
   * var resize = new api.Animation(buttonId, 250);
   * resize.setSize(buttonWidth, buttonHeight);
   * scene.addAnimation(resize);
   * scene.flush();
   */
  class Scene {

    constructor() {
      this.idIndex = api.getContentElementId() + 1;
      this.commands = [];
      this.elements = new Set();
      this.animations = {};
    }

    /**
     * Flush all queued commands to native.
     */
    flush() {
      api.sendCommands(this.commands);
      this.commands = [];
    }

    /**
     * Add a new UiElement to the scene, returning the ID assigned.
     */
    addElement(element) {
      var id = this.idIndex++;
      element.id = id;
      this.commands.push({
        'type': api.Command.ADD_ELEMENT,
        'data': element
      });
      this.elements.add(id);
      return id;
    }

    /**
     * Update an existing element, according to a UiElementUpdate object.
     */
    updateElement(id, update) {
      // To-do:  Make sure ID exists.
      update.id = id;
      this.commands.push({
        'type': api.Command.UPDATE_ELEMENT,
        'data': update
      });
    }

    /*
     * Remove an element from the scene.
     */
    removeElement(id) {
      // To-do: Make sure ID exists.
      this.commands.push({
        'type': api.Command.REMOVE_ELEMENT,
        'data': {'id': id}
      });
      this.elements.delete(id);
    }

    /**
     * Add a new Animation to the scene, returning the ID assigned.
     */
    addAnimation(animation) {
      var id = this.idIndex++;
      animation.id = id;
      this.commands.push({
        'type': api.Command.ADD_ANIMATION,
        'data': animation
      });
      this.animations[id] = animation.meshId;
      return id;
    }

    /*
     * Remove an animation from the scene.
     */
    removeAnimation(id) {
      // To-do: Make sure ID exists.
      this.commands.push({
        'type': api.Command.REMOVE_ANIMATION,
        'data': {'id': id, 'meshId': this.animations[id]}
      });
      delete this.animations[id];
    }

    /*
     * Purge all elements in the scene.
     */
    purge() {
      var ids = Object.keys(this.animations);
      for (let id_key of ids) {
        var id = parseInt(id_key);
        this.removeAnimation(id);
      }
      var ids = this.elements.values();
      for (let id of ids) {
        this.removeElement(id);
      }
      this.flush();
    }
  };

  return {
    Scene: Scene,
  };
})();
