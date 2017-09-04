// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var api = (function() {
  'use strict';

  /**
   * Enumeration of scene update commands.
   * @enum {number}
   * @const
   */
  var Command = Object.freeze({
    'ADD_ELEMENT': 0,
    'UPDATE_ELEMENT': 1,
    'REMOVE_ELEMENT': 2,
    'ADD_ANIMATION': 3,
    'REMOVE_ANIMATION': 4
  });

  /**
   * Sends one or more commands to native scene management. Commands are used
   * to add, modify or remove elements and animations. For examples of how to
   * format command parameters, refer to examples in scene.js.
   */
  function sendCommands(commands) {
    chrome.send('updateScene', commands);
  }

  /**
   * Returns the element ID of the browser content quad, allowing the content to
   * be manimulated in the scene.
   */
  function getContentElementId() {
    return 0;
  }

  /**
   * Enumeration of valid Anchroing for X axis.
   * An element can either be anchored to the left, right, or center of the main
   * content rect (or it can be absolutely positioned using NONE). Any
   * translations applied will be relative to this anchoring.
   * @enum {number}
   * @const
   */
  var XAnchoring = Object.freeze({
    'XNONE': 0,
    'XLEFT': 1,
    'XRIGHT': 2
  });

  /**
   * Enumeration of valid Anchroing for Y axis.
   * @enum {number}
   * @const
   */
  var YAnchoring = Object.freeze({
    'YNONE': 0,
    'YTOP': 1,
    'YBOTTOM': 2
  });

  /**
   * Enumeration of actions that can be triggered by the HTML UI.
   * @enum {number}
   * @const
   */
  var Action = Object.freeze({
    'HISTORY_BACK': 0,
    'HISTORY_FORWARD': 1,
    'RELOAD': 2,
    'ZOOM_OUT': 3,
    'ZOOM_IN': 4,
    'RELOAD_UI': 5,
  });

  /**
   * Enumeration of modes that can be specified by the native side.
   * @enum {number}
   * @const
   */
  var Mode = Object.freeze({
    'UNKNOWN': -1,
    'STANDARD': 0,
    'WEB_VR': 1
  });

  /**
   * Triggers an Action.
   */
  function doAction(action) {
    chrome.send('doAction', [action]);
  }

  /**
   * Notify native scene management that DOM loading has completed, at the
   * specified page size.
   */
  function domLoaded() {
    chrome.send('domLoaded');
  }

  /**
   * Represents updates to UI element properties. Any properties set on this
   * object are relayed to an underlying native element via scene command.
   * Properties that are not set on this object are left unchanged.
   */
  class UiElementUpdate {

    setIsContentQuad() {
      this.contentQuad = true;
    }

    /**
     * Specify a parent for this element. If set, this element is positioned
     * relative to its parent element, rather than absolutely. This allows
     * elements to automatically move with a parent.
     */
    setParentId(id) {
      this.parentId = id;
    }

    /**
     * Specify the width and height (in meters) of an element.
     */
    setSize(x, y) {
      this.size = { x: x, y: y };
    }

    /**
     * Specify optional scaling of the element, and any children.
     */
    setScale(x, y, z) {
      this.scale = { x: x, y: y, z: z };
    }

    /**
     * Specify rotation for the element. The rotation is specified in axis-angle
     * representation (rotate around unit vector [x, y, z] by 'a' radians).
     */
    setRotation(x, y, z, a) {
      this.rotation = { x: x, y: y, z: z, a: a };
    }

    /**
     * Specify the translation of the element. If anchoring is specified, the
     * offset is applied to the anchoring position rather than the origin.
     * Translation is applied after scaling and rotation.
     */
    setTranslation(x, y, z) {
      this.translation = { x: x, y: y, z: z };
    }

    /**
     * Anchoring allows a rectangle to be positioned relative to the edge of
     * its parent, without being concerned about the size of the parent.
     * Values should be XAnchoring and YAnchoring elements.
     * Example: element.setAnchoring(XAnchoring.XNONE, YAnchoring.YBOTTOM);
     */
    setAnchoring(x, y) {
      this.xAnchoring = x;
      this.yAnchoring = y;
    }

    /**
     * Visibility controls whether the element is rendered.
     */
    setVisible(visible) {
      this.visible = !!visible;
    }

    /**
     * Hit-testable implies that the reticle will hit the element, if visible.
     */
    setHitTestable(testable) {
      this.hitTestable = !!testable;
    }

    /**
     * Causes an element to be rendered relative to the field of view, rather
     * than the scene.  Elements locked in this way should not have a parent.
     */
    setLockToFieldOfView(locked) {
      this.lockToFov = !!locked;
    }
  };

  /**
   * Represents a new UI element. This object builds on UiElementUpdate,
   * forcing the underlying texture coordinates to be specified.
   */
  class UiElement extends UiElementUpdate {
    /**
     * Constructor of UiElement.
     * pixelX and pixelY values indicate the left upper corner; pixelWidth and
     * pixelHeight is width and height of the texture to be copied from the web
     * contents.
     */
    constructor(pixelX, pixelY, pixelWidth, pixelHeight) {
      super();

      this.copyRect = {
          x: pixelX,
          y: pixelY,
          width: pixelWidth,
          height: pixelHeight
      };
    }
  };

  /**
   * Enumeration of animatable properties.
   * @enum {number}
   * @const
   */
  var Property = Object.freeze({
    'COPYRECT': 0,
    'SIZE': 1,
    'TRANSLATION': 2,
    'ORIENTATION': 3,
    'ROTATION': 4
  });

  /**
   * Enumeration of easing type.
   * @enum {number}
   * @const
   */
  var Easing = Object.freeze({
    'LINEAR': 0,
    'CUBICBEZIER': 1,
    'EASEIN': 2,
    'EASEOUT': 3
  });

  /**
   * Base animation class. An animation can vary only one object property.
   */
  class Animation {
    constructor(elementId, durationMs) {
      this.meshId = elementId;
      this.to = {};
      this.easing = {};
      this.easing.type = api.Easing.LINEAR;

      // How many milliseconds in the future to start the animation.
      this.startInMillis = 0.0;

      // Duration of the animation (milliseconds).
      this.durationMillis = durationMs;
    }

    /**
     * Set the animation's final element size.
     */
    setSize(width, height) {
      this.property = api.Property.SIZE;
      this.to.x = width;
      this.to.y = height;
    }

    /**
     * Set the animation's final element scale.
     */
    setScale(x, y, z) {
      this.property = api.Property.SCALE;
      this.to.x = x;
      this.to.y = y;
      this.to.z = z;
    }

    /**
     * Set the animation's final element rotation.
     */
    setRotation(x, y, z, a) {
      this.property = api.Property.ROTATION;
      this.to.x = x;
      this.to.y = y;
      this.to.z = z;
      this.to.a = a;
    }

    /**
     * Set the animation's final element translation.
     */
    setTranslation(x, y, z) {
      this.property = api.Property.TRANSLATION;
      this.to.x = x;
      this.to.y = y;
      this.to.z = z;
    }
  };

  return {
    sendCommands: sendCommands,
    XAnchoring: XAnchoring,
    YAnchoring: YAnchoring,
    Property: Property,
    Easing: Easing,
    Command: Command,
    Action: Action,
    Mode: Mode,
    getContentElementId: getContentElementId,
    UiElement: UiElement,
    UiElementUpdate: UiElementUpdate,
    Animation: Animation,
    doAction: doAction,
    domLoaded: domLoaded,
  };
})();
