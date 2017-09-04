// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword.constants', function() {
'use strict';

/**
 * Number of seconds of audio to record when logging is enabled.
 * @const {number}
 */
var AUDIO_LOG_SECONDS = 2;

/**
 * Timeout in seconds, for detecting false positives with a hotword stream.
 * @const {number}
 */
var HOTWORD_STREAM_TIMEOUT_SECONDS = 2;

/**
 * Hotword data shared module extension's ID.
 * @const {string}
 */
var SHARED_MODULE_ID = 'lccekmodgklaepjeofjdjpbminllajkg';

/**
 * Path to shared module data.
 * @const {string}
 */
var SHARED_MODULE_ROOT = '_modules/' + SHARED_MODULE_ID;

/**
 * Name used by the content scripts to create communications Ports.
 * @const {string}
 */
var CLIENT_PORT_NAME = 'chwcpn';

/**
 * The field name to specify the command among pages.
 * @const {string}
 */
var COMMAND_FIELD_NAME = 'cmd';

/**
 * The speaker model file name.
 * @const {string}
 */
var SPEAKER_MODEL_FILE_NAME = 'speaker_model.data';

/**
 * The training utterance file name prefix.
 * @const {string}
 */
var UTTERANCE_FILE_PREFIX = 'utterance-';

/**
 * The training utterance file extension.
 * @const {string}
 */
var UTTERANCE_FILE_EXTENSION = '.raw';

/**
 * The number of training utterances required to train the speaker model.
 * @const {number}
 */
var NUM_TRAINING_UTTERANCES = 3;

/**
 * The size of the file system requested for reading the speaker model and
 * utterances. This number should always be larger than the combined file size,
 * currently 576338 bytes as of February 2015.
 * @const {number}
 */
var FILE_SYSTEM_SIZE_BYTES = 1048576;

/**
 * Time to wait for expected messages, in milliseconds.
 * @enum {number}
 */
var TimeoutMs = {
  SHORT: 200,
  NORMAL: 500,
  LONG: 2000
};

/**
 * The URL of the files used by the plugin.
 * @enum {string}
 */
var File = {
  RECOGNIZER_CONFIG: 'hotword.data',
};

/**
 * Errors emitted by the NaClManager.
 * @enum {string}
 */
var Error = {
  NACL_CRASH: 'nacl_crash',
  TIMEOUT: 'timeout',
};

/**
 * Event types supported by NaClManager.
 * @enum {string}
 */
var Event = {
  READY: 'ready',
  TRIGGER: 'trigger',
  SPEAKER_MODEL_SAVED: 'speaker model saved',
  ERROR: 'error',
  TIMEOUT: 'timeout',
};

/**
 * Messages for communicating with the NaCl recognizer plugin. These must match
 * constants in <google3>/hotword_plugin.c
 * @enum {string}
 */
var NaClPlugin = {
  RESTART: 'r',
  SAMPLE_RATE_PREFIX: 'h',
  MODEL_PREFIX: 'm',
  STOP: 's',
  LOG: 'l',
  DSP: 'd',
  BEGIN_SPEAKER_MODEL: 'b',
  ADAPT_SPEAKER_MODEL: 'a',
  FINISH_SPEAKER_MODEL: 'f',
  SPEAKER_MODEL_SAVED: 'sm_saved',
  REQUEST_MODEL: 'model',
  MODEL_LOADED: 'model_loaded',
  READY_FOR_AUDIO: 'audio',
  STOPPED: 'stopped',
  HOTWORD_DETECTED: 'hotword',
  MS_CONFIGURED: 'ms_configured',
  TIMEOUT: 'timeout'
};

/**
 * Messages sent from the injected scripts to the Google page.
 * @enum {string}
 */
var CommandToPage = {
  HOTWORD_VOICE_TRIGGER: 'vt',
  HOTWORD_STARTED: 'hs',
  HOTWORD_ENDED: 'hd',
  HOTWORD_TIMEOUT: 'ht',
  HOTWORD_ERROR: 'he'
};

/**
 * Messages sent from the Google page to the extension or to the
 * injected script and then passed to the extension.
 * @enum {string}
 */
var CommandFromPage = {
  SPEECH_START: 'ss',
  SPEECH_END: 'se',
  SPEECH_RESET: 'sr',
  SHOWING_HOTWORD_START: 'shs',
  SHOWING_ERROR_MESSAGE: 'sem',
  SHOWING_TIMEOUT_MESSAGE: 'stm',
  CLICKED_RESUME: 'hcc',
  CLICKED_RESTART: 'hcr',
  CLICKED_DEBUG: 'hcd',
  WAKE_UP_HELPER: 'wuh',
  // Command specifically for the opt-in promo below this line.
  // User has explicitly clicked 'no'.
  CLICKED_NO_OPTIN: 'hcno',
  // User has opted in.
  CLICKED_OPTIN: 'hco',
  // User clicked on the microphone.
  PAGE_WAKEUP: 'wu'
};

/**
 * Source of a hotwording session request.
 * @enum {string}
 */
var SessionSource = {
  LAUNCHER: 'launcher',
  NTP: 'ntp',
  ALWAYS: 'always',
  TRAINING: 'training'
};

/**
 * The mode to start the hotword recognizer in.
 * @enum {string}
 */
var RecognizerStartMode = {
  NORMAL: 'normal',
  NEW_MODEL: 'new model',
  ADAPT_MODEL: 'adapt model'
};

/**
 * MediaStream open success/errors to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaMediaStreamOpenResult = {
  SUCCESS: 0,
  UNKNOWN: 1,
  NOT_SUPPORTED: 2,
  PERMISSION_DENIED: 3,
  CONSTRAINT_NOT_SATISFIED: 4,
  OVERCONSTRAINED: 5,
  NOT_FOUND: 6,
  ABORT: 7,
  SOURCE_UNAVAILABLE: 8,
  PERMISSION_DISMISSED: 9,
  INVALID_STATE: 10,
  DEVICES_NOT_FOUND: 11,
  INVALID_SECURITY_ORIGIN: 12,
  MAX: 12
};

/**
 * UMA metrics.
 * DO NOT change these enum values.
 * @enum {string}
 */
var UmaMetrics = {
  TRIGGER: 'Hotword.HotwordTrigger',
  MEDIA_STREAM_RESULT: 'Hotword.HotwordMediaStreamResult',
  NACL_PLUGIN_LOAD_RESULT: 'Hotword.HotwordNaClPluginLoadResult',
  NACL_MESSAGE_TIMEOUT: 'Hotword.HotwordNaClMessageTimeout',
  TRIGGER_SOURCE: 'Hotword.HotwordTriggerSource'
};

/**
 * Message waited for by NaCl plugin, to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaNaClMessageTimeout = {
  REQUEST_MODEL: 0,
  MODEL_LOADED: 1,
  READY_FOR_AUDIO: 2,
  STOPPED: 3,
  HOTWORD_DETECTED: 4,
  MS_CONFIGURED: 5,
  MAX: 5
};

/**
 * NaCl plugin load success/errors to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaNaClPluginLoadResult = {
  SUCCESS: 0,
  UNKNOWN: 1,
  CRASH: 2,
  NO_MODULE_FOUND: 3,
  MAX: 3
};

/**
 * Source of hotword triggering, to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaTriggerSource = {
  LAUNCHER: 0,
  NTP_GOOGLE_COM: 1,
  ALWAYS_ON: 2,
  TRAINING: 3,
  MAX: 3
};

/**
 * The browser UI language.
 * @const {string}
 */
var UI_LANGUAGE = (chrome.i18n && chrome.i18n.getUILanguage) ?
      chrome.i18n.getUILanguage() : '';

return {
  AUDIO_LOG_SECONDS: AUDIO_LOG_SECONDS,
  CLIENT_PORT_NAME: CLIENT_PORT_NAME,
  COMMAND_FIELD_NAME: COMMAND_FIELD_NAME,
  FILE_SYSTEM_SIZE_BYTES: FILE_SYSTEM_SIZE_BYTES,
  HOTWORD_STREAM_TIMEOUT_SECONDS: HOTWORD_STREAM_TIMEOUT_SECONDS,
  NUM_TRAINING_UTTERANCES: NUM_TRAINING_UTTERANCES,
  SHARED_MODULE_ID: SHARED_MODULE_ID,
  SHARED_MODULE_ROOT: SHARED_MODULE_ROOT,
  SPEAKER_MODEL_FILE_NAME: SPEAKER_MODEL_FILE_NAME,
  UI_LANGUAGE: UI_LANGUAGE,
  UTTERANCE_FILE_EXTENSION: UTTERANCE_FILE_EXTENSION,
  UTTERANCE_FILE_PREFIX: UTTERANCE_FILE_PREFIX,
  CommandToPage: CommandToPage,
  CommandFromPage: CommandFromPage,
  Error: Error,
  Event: Event,
  File: File,
  NaClPlugin: NaClPlugin,
  RecognizerStartMode: RecognizerStartMode,
  SessionSource: SessionSource,
  TimeoutMs: TimeoutMs,
  UmaMediaStreamOpenResult: UmaMediaStreamOpenResult,
  UmaMetrics: UmaMetrics,
  UmaNaClMessageTimeout: UmaNaClMessageTimeout,
  UmaNaClPluginLoadResult: UmaNaClPluginLoadResult,
  UmaTriggerSource: UmaTriggerSource
};

});
