// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This provides the different code types for the gnubby
 * operations.
 */

var GnubbyCodeTypes = {};

/**
 * Request succeeded.
 * @const
 */
GnubbyCodeTypes.OK = 0;

/**
 * All plugged in devices are already enrolled.
 * @const
 */
GnubbyCodeTypes.ALREADY_ENROLLED = 2;

/**
 * None of the plugged in devices are enrolled.
 * @const
 */
GnubbyCodeTypes.NONE_PLUGGED_ENROLLED = 3;

/**
 * One or more devices are waiting for touch.
 * @const
 */
GnubbyCodeTypes.WAIT_TOUCH = 4;

/**
 * No gnubbies found.
 * @const
 */
GnubbyCodeTypes.NO_GNUBBIES = 5;

/**
 * Unknown error during enrollment.
 * @const
 */
GnubbyCodeTypes.UNKNOWN_ERROR = 7;

/**
 * Extension not found.
 * @const
 */
GnubbyCodeTypes.NO_EXTENSION = 8;

// TODO: change to none_enrolled_for_account and none_enrolled_present
/**
 * No devices enrolled for this user.
 * @const
 */
GnubbyCodeTypes.NO_DEVICES_ENROLLED = 9;

/**
 * gnubby errors due to chrome issues
 * @const
 */
GnubbyCodeTypes.BROWSER_ERROR = 10;

/**
 * gnubbyd taking too long
 * @const
 */
GnubbyCodeTypes.LONG_WAIT = 11;

/**
 * Bad request.
 * @const
 */
GnubbyCodeTypes.BAD_REQUEST = 12;

/**
 * All gnubbies are too busy to handle your request.
 * @const
 */
GnubbyCodeTypes.BUSY = 13;

/**
 * There is a bad app id in the request.
 * @const
 */
GnubbyCodeTypes.BAD_APP_ID = 14;
