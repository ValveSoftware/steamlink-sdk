// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Accesses Chrome's accessibility extension API and gives
 * spoken feedback for events that happen in the "Chrome of Chrome".
 *
 */

goog.provide('cvox.AccessibilityApiHandler');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.AbstractTts');
goog.require('cvox.BrailleInterface');
goog.require('cvox.BrailleUtil');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.NavBraille');


/**
 * The chrome.experimental.accessibility API is moving to
 * chrome.accessibilityPrivate, so provide an alias during the transition.
 *
 * TODO(dmazzoni): Remove after the stable version of Chrome no longer
 * has the experimental accessibility API.
 */
chrome.experimental = chrome.experimental || {};
/**
 * Fall back on the experimental API if the new name is not available.
 */
chrome.accessibilityPrivate = chrome.accessibilityPrivate ||
    chrome.experimental.accessibility;


/**
 * Class that adds listeners and handles events from the accessibility API.
 * @constructor
 * @implements {cvox.TtsCapturingEventListener}
 * @param {cvox.TtsInterface} tts The TTS to use for speaking.
 * @param {cvox.BrailleInterface} braille The braille interface to use for
 * brailing.
 * @param {Object} earcons The earcons object to use for playing
 *        earcons.
 */
cvox.AccessibilityApiHandler = function(tts, braille, earcons) {
  this.tts = tts;
  this.braille = braille;
  this.earcons = earcons;
  /**
   * Tracks the previous description received.
   * @type {Object}
   * @private
   */
  this.prevDescription_ = {};
  /**
   * Array of strings to speak the next time TTS is idle.
   * @type {!Array.<string>}
   * @private
   */
  this.idleSpeechQueue_ = [];

  try {
    chrome.accessibilityPrivate.setAccessibilityEnabled(true);
    chrome.accessibilityPrivate.setNativeAccessibilityEnabled(
        !cvox.ChromeVox.isActive);
    this.addEventListeners_();
    if (cvox.ChromeVox.isActive) {
      this.queueAlertsForActiveTab();
    }
  } catch (err) {
    console.log('Error trying to access accessibility extension api.');
  }
};

/**
 * The interface used to manage speech.
 * @type {cvox.TtsInterface}
 */
cvox.AccessibilityApiHandler.prototype.tts = null;

/**
 * The interface used to manage braille.
 * @type {cvox.BrailleInterface}
 */
cvox.AccessibilityApiHandler.prototype.braille = null;

/**
 * The object used to manage arcons.
 * @type Object
 */
cvox.AccessibilityApiHandler.prototype.earcons = null;

/**
 * The object that can describe changes and cursor movement in a generic
 *     editable text field.
 * @type {Object}
 */
cvox.AccessibilityApiHandler.prototype.editableTextHandler = null;

/**
 * The name of the editable text field associated with
 * |editableTextHandler|, so we can tell when focus moves.
 * @type {string}
 */
cvox.AccessibilityApiHandler.prototype.editableTextName = '';

/**
 * The queue mode for the next focus event.
 * @type {number}
 */
cvox.AccessibilityApiHandler.prototype.nextQueueMode = 0;

/**
 * The timeout id for the pending text changed event - the return
 * value from window.setTimeout. We need to delay text events slightly
 * and return only the last one because sometimes we get a rapid
 * succession of related events that should all be considered one
 * bulk change - in particular, autocomplete in the location bar comes
 * as multiple events in a row.
 * @type {?number}
 */
cvox.AccessibilityApiHandler.prototype.textChangeTimeout = null;

/**
 * Most controls have a "context" - the name of the window, dialog, toolbar,
 * or menu they're contained in. We announce a context once, when you
 * first enter it - and we don't announce it again when you move to something
 * else within the same context. This variable keeps track of the most
 * recent context.
 * @type {?string}
 */
cvox.AccessibilityApiHandler.prototype.lastContext = null;

/**
 * Delay in ms between when a text event is received and when it's spoken.
 * @type {number}
 * @const
 */
cvox.AccessibilityApiHandler.prototype.TEXT_CHANGE_DELAY = 10;

/**
 * ID returned from setTimeout to queue up speech on idle.
 * @type {?number}
 * @private
 */
cvox.AccessibilityApiHandler.prototype.idleSpeechTimeout_ = null;

/**
 * Milliseconds of silence to wait before considering speech to be idle.
 * @const
 */
cvox.AccessibilityApiHandler.prototype.IDLE_SPEECH_DELAY_MS = 500;

/**
 * Called to let us know that the last speech came from web, and not from
 * native UI. Clear the context and any state associated with the last
 * focused control.
 */
cvox.AccessibilityApiHandler.prototype.setWebContext = function() {
  // This will never be spoken - it's just supposed to be a string that
  // won't match the context of the next control that gets focused.
  this.lastContext = '--internal-web--';
  this.editableTextHandler = null;
  this.editableTextName = '';
};

/**
 * Adds event listeners.
 * @private
 */
cvox.AccessibilityApiHandler.prototype.addEventListeners_ = function() {
  /** Alias getMsg as msg. */
  var msg = goog.bind(cvox.ChromeVox.msgs.getMsg, cvox.ChromeVox.msgs);

  var accessibility = chrome.accessibilityPrivate;

  chrome.tabs.onCreated.addListener(goog.bind(function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    this.tts.speak(msg('chrome_tab_created'),
                   0,
                   cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
    this.braille.write(cvox.NavBraille.fromText(msg('chrome_tab_created')));
    this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_OPEN);
  }, this));

  chrome.tabs.onRemoved.addListener(goog.bind(function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_CLOSE);
  }, this));

  chrome.tabs.onActivated.addListener(goog.bind(function(activeInfo) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    chrome.tabs.get(activeInfo.tabId, goog.bind(function(tab) {
      if (tab.status == 'loading') {
        return;
      }
      var title = tab.title ? tab.title : tab.url;
      this.tts.speak(msg('chrome_tab_selected',
                         [title]),
                     cvox.AbstractTts.QUEUE_MODE_FLUSH,
                     cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
      this.braille.write(
          cvox.NavBraille.fromText(msg('chrome_tab_selected', [title])));
      this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_SELECT);
      this.queueAlertsForActiveTab();
    }, this));
  }, this));

  chrome.tabs.onUpdated.addListener(goog.bind(function(tabId, selectInfo) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    chrome.tabs.get(tabId, goog.bind(function(tab) {
      if (!tab.active) {
        return;
      }
      if (tab.status == 'loading') {
        this.earcons.playEarcon(cvox.AbstractEarcons.BUSY_PROGRESS_LOOP);
      } else {
        this.earcons.playEarcon(cvox.AbstractEarcons.TASK_SUCCESS);
      }
    }, this));
  }, this));

  chrome.windows.onFocusChanged.addListener(goog.bind(function(windowId) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    if (windowId == chrome.windows.WINDOW_ID_NONE) {
      return;
    }
    chrome.windows.get(windowId, goog.bind(function(window) {
      chrome.tabs.getSelected(windowId, goog.bind(function(tab) {
        var msgId = window.incognito ? 'chrome_incognito_window_selected' :
          'chrome_normal_window_selected';
        var title = tab.title ? tab.title : tab.url;
        this.tts.speak(msg(msgId, [title]),
                       cvox.AbstractTts.QUEUE_MODE_FLUSH,
                       cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
        this.braille.write(cvox.NavBraille.fromText(msg(msgId, [title])));
        this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_SELECT);
      }, this));
    }, this));
  }, this));

  chrome.accessibilityPrivate.onWindowOpened.addListener(
      goog.bind(function(win) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    this.tts.speak(win.name,
                   cvox.AbstractTts.QUEUE_MODE_FLUSH,
                   cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
    this.braille.write(cvox.NavBraille.fromText(win.name));
    // Queue the next utterance because a window opening is always followed
    // by a focus event.
    this.nextQueueMode = 1;
    this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_OPEN);
    this.queueAlertsForActiveTab();
  }, this));

  chrome.accessibilityPrivate.onWindowClosed.addListener(
      goog.bind(function(win) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    // Don't speak, just play the earcon.
    this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_CLOSE);
  }, this));

  chrome.accessibilityPrivate.onMenuOpened.addListener(
      goog.bind(function(menu) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    this.tts.speak(msg('chrome_menu_opened', [menu.name]),
                   cvox.AbstractTts.QUEUE_MODE_FLUSH,
                   cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
    this.braille.write(
        cvox.NavBraille.fromText(msg('chrome_menu_opened', [menu.name])));
    this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_OPEN);
  }, this));

  chrome.accessibilityPrivate.onMenuClosed.addListener(
      goog.bind(function(menu) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    // Don't speak, just play the earcon.
    this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_CLOSE);
  }, this));

  // systemPrivate API is only available when this extension is loaded as a
  // component extension embedded in Chrome.
  chrome.permissions.contains(
      { permissions: ['systemPrivate'] },
      goog.bind(function(result) {
    if (!result) {
      return;
    }

    // TODO(plundblad): Remove when the native sound is turned on by default.
    // See crbug.com:225886.
    var addOnVolumeChangedListener = goog.bind(function() {
      chrome.systemPrivate.onVolumeChanged.addListener(goog.bind(
          function(volume) {
        if (!cvox.ChromeVox.isActive) {
          return;
        }
        // Don't speak, just play the earcon.
        this.earcons.playEarcon(cvox.AbstractEarcons.TASK_SUCCESS);
      }, this));
    }, this);
    if (chrome.commandLinePrivate) {
      chrome.commandLinePrivate.hasSwitch('disable-volume-adjust-sound',
          goog.bind(function(result) {
        if (result) {
          addOnVolumeChangedListener();
        }
      }, this));
    } else {
      addOnVolumeChangedListener();
    }

    chrome.systemPrivate.onBrightnessChanged.addListener(
        goog.bind(
        /**
         * @param {{brightness: number, userInitiated: boolean}} brightness
         */
        function(brightness) {
          if (brightness.userInitiated) {
            this.earcons.playEarcon(cvox.AbstractEarcons.TASK_SUCCESS);
            this.tts.speak(
                msg('chrome_brightness_changed', [brightness.brightness]),
                cvox.AbstractTts.QUEUE_MODE_FLUSH,
                cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
            this.braille.write(cvox.NavBraille.fromText(
                msg('chrome_brightness_changed', [brightness.brightness])));
          }
        }, this));

    chrome.systemPrivate.onScreenUnlocked.addListener(goog.bind(function() {
      chrome.systemPrivate.getUpdateStatus(goog.bind(function(status) {
        if (!cvox.ChromeVox.isActive) {
          return;
        }
        // Speak about system update when it's ready, otherwise speak nothing.
        if (status.state == 'NeedRestart') {
          this.tts.speak(msg('chrome_system_need_restart'),
                         cvox.AbstractTts.QUEUE_MODE_FLUSH,
                         cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
          this.braille.write(
              cvox.NavBraille.fromText(msg('chrome_system_need_restart')));
        }
      }, this));
    }, this));

    chrome.systemPrivate.onWokeUp.addListener(goog.bind(function() {
      if (!cvox.ChromeVox.isActive) {
        return;
      }
      // Don't speak, just play the earcon.
      this.earcons.playEarcon(cvox.AbstractEarcons.OBJECT_OPEN);
    }, this));
  }, this));

  chrome.accessibilityPrivate.onControlFocused.addListener(
      goog.bind(this.onControlFocused, this));

  chrome.accessibilityPrivate.onControlAction.addListener(
      goog.bind(function(ctl) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }

    var description = this.describe(ctl, true);
    this.tts.speak(description.utterance,
                   cvox.AbstractTts.QUEUE_MODE_FLUSH,
                   description.ttsProps);
    description.braille.write();
    if (description.earcon) {
      this.earcons.playEarcon(description.earcon);
    }
  }, this));

  try {
    chrome.accessibilityPrivate.onControlHover.addListener(
        goog.bind(function(ctl) {
      if (!cvox.ChromeVox.isActive) {
        return;
      }

      var hasTouch = 'ontouchstart' in window;
      if (!hasTouch) {
        return;
      }

      var description = this.describe(ctl, false);
      this.tts.speak(description.utterance,
                     cvox.AbstractTts.QUEUE_MODE_FLUSH,
                     description.ttsProps);
      description.braille.write();
      if (description.earcon) {
        this.earcons.playEarcon(description.earcon);
      }
    }, this));
  } catch (e) {}

  chrome.accessibilityPrivate.onTextChanged.addListener(
       goog.bind(function(ctl) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }

    if (!this.editableTextHandler ||
        this.editableTextName != ctl.name ||
        this.lastContext != ctl.context) {
      // Chrome won't send a text change event on a control that isn't
      // focused. If we get a text change event and it doesn't match the
      // focused control, treat it as a focus event initially.
      this.onControlFocused(ctl);
      return;
    }

    // Only send the most recent text changed event - throw away anything
    // that was pending.
    if (this.textChangeTimeout) {
      window.clearTimeout(this.textChangeTimeout);
    }

    // Handle the text change event after a small delay, so multiple
    // events in rapid succession are handled as a single change. This is
    // specifically for the location bar with autocomplete - typing a
    // character and getting the autocompleted text and getting that
    // text selected may be three separate events.
    this.textChangeTimeout = window.setTimeout(
        goog.bind(function() {
          var textChangeEvent = new cvox.TextChangeEvent(
              ctl.details.value,
              ctl.details.selectionStart,
              ctl.details.selectionEnd,
              true);  // triggered by user
          this.editableTextHandler.changed(
              textChangeEvent);
          this.describe(ctl, false).braille.write();
        }, this), this.TEXT_CHANGE_DELAY);
  }, this));

  this.tts.addCapturingEventListener(this);
};

/**
 * Handle the feedback when a new control gets focus.
 * @param {AccessibilityObject} ctl The focused control.
 */
cvox.AccessibilityApiHandler.prototype.onControlFocused = function(ctl) {
  if (!cvox.ChromeVox.isActive) {
    return;
  }

  // Call this first because it may clear this.editableTextHandler.
  var description = this.describe(ctl, false);

  if (ctl.type == 'textbox') {
    var start = ctl.details.selectionStart;
    var end = ctl.details.selectionEnd;
    if (start > end) {
      start = ctl.details.selectionEnd;
      end = ctl.details.selectionStart;
    }
    this.editableTextName = ctl.name;
    this.editableTextHandler =
        new cvox.ChromeVoxEditableTextBase(
            ctl.details.value,
            start,
            end,
            ctl.details.isPassword,
            this.tts);
  } else {
    this.editableTextHandler = null;
  }

  this.tts.speak(description.utterance,
                 this.nextQueueMode,
                 description.ttsProps);
  description.braille.write();
  this.nextQueueMode = 0;
  if (description.earcon) {
    this.earcons.playEarcon(description.earcon);
  }
};

/**
 * Called when any speech starts.
 */
cvox.AccessibilityApiHandler.prototype.onTtsStart = function() {
  if (this.idleSpeechTimeout_) {
    window.clearTimeout(this.idleSpeechTimeout_);
  }
};

/**
 * Called when any speech ends.
 */
cvox.AccessibilityApiHandler.prototype.onTtsEnd = function() {
  if (this.idleSpeechQueue_.length > 0) {
    this.idleSpeechTimeout_ = window.setTimeout(
        goog.bind(this.onTtsIdle, this),
        this.IDLE_SPEECH_DELAY_MS);
  }
};

/**
 * Called when speech has been idle for a certain minimum delay.
 * Speaks queued messages.
 */
cvox.AccessibilityApiHandler.prototype.onTtsIdle = function() {
  if (this.idleSpeechQueue_.length == 0) {
    return;
  }
  var utterance = this.idleSpeechQueue_.shift();
  var msg = goog.bind(cvox.ChromeVox.msgs.getMsg, cvox.ChromeVox.msgs);
  this.tts.speak(utterance,
                 cvox.AbstractTts.QUEUE_MODE_FLUSH,
                 cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
};

/**
 * Given a control received from the accessibility api, determine an
 * utterance to speak, text to braille, and an earcon to play to describe it.
 * @param {Object} control The control that had an action performed on it.
 * @param {boolean} isSelect True if the action is a select action,
 *     otherwise it's a focus action.
 * @return {Object} An object containing a string field |utterance|, object
 *      |ttsProps|, |braille|, and earcon |earcon|.
 */
cvox.AccessibilityApiHandler.prototype.describe = function(control, isSelect) {
  /** Alias getMsg as msg. */
  var msg = goog.bind(cvox.ChromeVox.msgs.getMsg, cvox.ChromeVox.msgs);

  var s = '';
  var braille = {};
  var ttsProps = cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT;

  var context = control.context;
  if (context && context != this.lastContext) {
    s += context + ', ';
    this.lastContext = context;
    this.editableTextHandler = null;
  }

  var earcon = undefined;
  var name = control.name.replace(/[_&]+/g, '').replace('...', '');
  braille.name = control.name;
  switch (control.type) {
    case 'checkbox':
      braille.roleMsg = 'input_type_checkbox';
      if (control.details.isChecked) {
        earcon = cvox.AbstractEarcons.CHECK_ON;
        s += msg('describe_checkbox_checked', [name]);
        braille.state = msg('checkbox_checked_state_brl');
      } else {
        earcon = cvox.AbstractEarcons.CHECK_OFF;
        s += msg('describe_checkbox_unchecked', [name]);
        braille.state = msg('checkbox_unchecked_state_brl');
      }
      break;
    case 'radiobutton':
      s += name;
      braille.roleMsg = 'input_type_radio';
      if (control.details.isChecked) {
        earcon = cvox.AbstractEarcons.CHECK_ON;
        s += msg('describe_radio_selected', [name]);
        braille.state = msg('radio_selected_state_brl');
      } else {
        earcon = cvox.AbstractEarcons.CHECK_OFF;
        s += msg('describe_radio_unselected', [name]);
        braille.state = msg('radio_unselected_state_brl');
      }
      break;
    case 'menu':
      s += msg('describe_menu', [name]);
      braille.roleMsg = 'aria_role_menu';
      break;
    case 'menuitem':
      s += msg(
          control.details.hasSubmenu ?
              'describe_menu_item_with_submenu' : 'describe_menu_item', [name]);
      braille.roleMsg = 'aria_role_menuitem';
      if (control.details.hasSubmenu) {
        braille.state = msg('aria_has_submenu_brl');
      }
      break;
    case 'window':
      s += msg('describe_window', [name]);
      // No specialization for braille.
      braille.name = s;
      break;
    case 'alert':
      earcon = cvox.AbstractEarcons.ALERT_NONMODAL;
      s += msg('aria_role_alert') + ': ' + name;
      ttsProps = cvox.AbstractTts.PERSONALITY_SYSTEM_ALERT;
      braille.roleMsg = 'aria_role_alert';
      isSelect = false;
      break;
    case 'textbox':
      earcon = cvox.AbstractEarcons.EDITABLE_TEXT;
      var unnamed = name == '' ? 'unnamed_' : '';
      var type, value;
      if (control.details.isPassword) {
        type = 'password';
        braille.roleMsg = 'input_type_password';
        value = control.details.value.replace(/./g, '*');
      } else {
        type = 'textbox';
        braille.roleMsg = 'input_type_text';
        value = control.details.value;
      }
      s += msg('describe_' + unnamed + type, [value, name]);
      braille.value = cvox.BrailleUtil.createValue(
          value, control.details.selectionStart, control.details.selectionEnd);
      break;
    case 'button':
      earcon = cvox.AbstractEarcons.BUTTON;
      s += msg('describe_button', [name]);
      braille.roleMsg = 'tag_button';
      break;
    case 'combobox':
    case 'listbox':
      earcon = cvox.AbstractEarcons.LISTBOX;
      var unnamed = name == '' ? 'unnamed_' : '';
      s += msg('describe_' + unnamed + control.type,
                            [control.details.value, name]);
      braille.roleMsg = 'tag_select';
      break;
    case 'link':
      earcon = cvox.AbstractEarcons.LINK;
      s += msg('describe_link', [name]);
      braille.roleMsg = 'tag_link';
      break;
    case 'tab':
      s += msg('describe_tab', [name]);
      braille.roleMsg = 'aria_role_tab';
      break;
    case 'slider':
      s += msg('describe_slider', [control.details.stringValue, name]);
      braille.value = cvox.BrailleUtil.createValue(control.details.stringValue);
      braille.roleMsg = 'aria_role_slider';
      break;
    case 'treeitem':
      if (this.prevDescription_ &&
          this.prevDescription_.details &&
          goog.isDef(control.details.itemDepth) &&
          this.prevDescription_.details.itemDepth !=
              control.details.itemDepth) {
        s += msg('describe_depth', [control.details.itemDepth]);
      }
      s += name + ' ' + msg('aria_role_treeitem');
      s += control.details.isItemExpanded ?
          msg('aria_expanded_true') : msg('aria_expanded_false');

      braille.name = Array(control.details.itemDepth).join(' ') + braille.name;
      braille.roleMsg = 'aria_role_treeitem';
      braille.state = control.details.isItemExpanded ?
          msg('aria_expanded_true_brl') : msg('aria_expanded_false_brl');
      break;

    default:
      s += name + ', ' + control.type;
      braille.role = control.type;
  }

  if (isSelect && control.type != 'slider') {
    s += msg('describe_selected');
  }
  if (control.details && control.details.itemCount >= 0) {
    s += msg('describe_index',
        [control.details.itemIndex + 1, control.details.itemCount]);
    braille.state = braille.state ? braille.state + ' ' : '';
    braille.state += msg('LIST_POSITION_BRL',
        [control.details.itemIndex + 1, control.details.itemCount]);
  }

  var description = {};
  description.utterance = s;
  description.ttsProps = ttsProps;
  var spannable = cvox.BrailleUtil.getTemplated(null, null, braille);
  var valueSelectionSpan = spannable.getSpanInstanceOf(
      cvox.BrailleUtil.ValueSelectionSpan);
  var brailleObj = {text: spannable};
  if (valueSelectionSpan) {
    brailleObj.startIndex = spannable.getSpanStart(valueSelectionSpan);
    brailleObj.endIndex = spannable.getSpanEnd(valueSelectionSpan);
  }
  description.braille = new cvox.NavBraille(brailleObj);
  description.earcon = earcon;
  this.prevDescription_ = control;
  return description;
};

/**
 * Queues alerts for the active tab, if any, which will be spoken
 * as soon as speech is idle.
 */
cvox.AccessibilityApiHandler.prototype.queueAlertsForActiveTab = function() {
  this.idleSpeechQueue_.length = 0;
  var msg = goog.bind(cvox.ChromeVox.msgs.getMsg, cvox.ChromeVox.msgs);

  chrome.tabs.query({'active': true, 'currentWindow': true},
      goog.bind(function(tabs) {
    if (tabs.length < 1) {
      return;
    }
    chrome.accessibilityPrivate.getAlertsForTab(
        tabs[0].id, goog.bind(function(alerts) {
      if (alerts.length == 0) {
        return;
      }

      var utterance = '';

      if (alerts.length == 1) {
        utterance += msg('page_has_one_alert_singular');
      } else {
        utterance += msg('page_has_alerts_plural',
                         [alerts.length]);
      }

      for (var i = 0; i < alerts.length; i++) {
        utterance += ' ' + alerts[i].message;
      }

      utterance += ' ' + msg('review_alerts');

      if (this.idleSpeechQueue_.indexOf(utterance) == -1) {
        this.idleSpeechQueue_.push(utterance);
      }
    }, this));
  }, this));
};
