// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var RowAlignment = {
  STRETCH: "stretch",
  LEFT: "left",
  RIGHT: "right",
  CENTER: "center",
}

/**
 * Ratio of key height and font size.
 * @type {number}
 */
var FONT_SIZE_RATIO = 2.5;

/**
 * @type {enum}
 * Possible layout alignments.
 */
var LayoutAlignment = {
  CENTER: "center",
  STRETCH: "stretch",
};

/**
 * The enumerations of key sounds.
 * @const
 * @type {enum}
 */
var Sound = {
  NONE: "none",
  DEFAULT: "keypress-standard",
};

/**
 * The enumeration of swipe directions.
 * @const
 * @type {Enum}
 */
var SwipeDirection = {
  RIGHT: 0x1,
  LEFT: 0x2,
  UP: 0x4,
  DOWN: 0x8
};

/**
 * The ratio between the width and height of the key when in portrait mode.
 * @type {number}
 */
var KEY_ASPECT_RATIO_PORTRAIT = 1;

/**
 * The ratio between the width and height of the key when in landscape mode.
 * @type {number}
 */
var KEY_ASPECT_RATIO_LANDSCAPE = 1.46;

/**
 * The ratio between the height and width of the compact keyboard.
 * @type {number}
 */
var DEFAULT_KEYBOARD_ASPECT_RATIO = 0.41;

/**
 * The default weight of a key.
 * @type {number}
 */
var DEFAULT_KEY_WEIGHT = 100;

/**
 * The default volume for keyboard sounds.
 * @type {number}
 */
var DEFAULT_VOLUME = 0.2;

/**
 * The top padding on each key.
 * @type {number}
 */
// TODO(rsadam): Remove this variable once figure out how to calculate this
// number before the key is rendered.
var KEY_PADDING_TOP = 1;
var KEY_PADDING_BOTTOM = 1;

/**
 * The greatest distance between a key and a touch point for a PointerEvent
 * to be processed.
 * @type {number}
 */
var MAX_TOUCH_FUZZ_DISTANCE = 20;

/**
 * The maximum number of extra pixels before a resize is triggered.
 * @type {number}
 */
var RESIZE_THRESHOLD = 20;

/**
 * The size of the pool to use for playing audio sounds on key press. This is to
 * enable the same sound to be overlayed, for example, when a repeat key is
 * pressed.
 * @type {number}
 */
var SOUND_POOL_SIZE = 10;

/**
 * Whether or not to enable sounds on key press.
 * @type {boolean}
 */
var SOUND_ENABLED = false;
