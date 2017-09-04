// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "mojo/public/js/support"
//
// Note: This file is for documentation purposes only. The code here is not
// actually executed. The real module is implemented natively in Mojo.

while (1);

/* @deprecated Please use watch()/cancelWatch() instead of
 *     asyncWait()/cancelWait().
 *
 * Waits on the given handle until the state indicated by |signals| is
 * satisfied.
 *
 * @param {MojoHandle} handle The handle to wait on.
 * @param {MojoHandleSignals} signals Specifies the condition to wait for.
 * @param {function (mojoResult)} callback Called with the result the wait is
 *     complete. See MojoWait for possible result codes.
 *
 * @return {MojoWaitId} A waitId that can be passed to cancelWait to cancel the
 *     wait.
 */
function asyncWait(handle, signals, callback) { [native code] }

/* @deprecated Please use watch()/cancelWatch() instead of
 *     asyncWait()/cancelWait().
 *
 * Cancels the asyncWait operation specified by the given |waitId|.
 *
 * @param {MojoWaitId} waitId The waitId returned by asyncWait.
 */
function cancelWait(waitId) { [native code] }

/* Begins watching a handle for |signals| to be satisfied or unsatisfiable.
 *
  * @param {MojoHandle} handle The handle to watch.
  * @param {MojoHandleSignals} signals The signals to watch.
  * @param {function (mojoResult)} calback Called with a result any time
  *     the watched signals become satisfied or unsatisfiable.
  *
  * @param {MojoWatchId} watchId An opaque identifier that identifies this
  *     watch.
  */
function watch(handle, signals, callback) { [native code] }

/* Cancels a handle watch initiated by watch().
 *
 * @param {MojoWatchId} watchId The watch identifier returned by watch().
 */
function cancelWatch(watchId) { [native code] }
