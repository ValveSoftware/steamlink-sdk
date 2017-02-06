// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: metricsPrivate */

/**
 * Describes the type of metric that is to be collected.
 * @typedef {{
 *   metricName: string,
 *   type: string,
 *   min: number,
 *   max: number,
 *   buckets: number
 * }}
 */
var MetricType;

/**
 * @const
 */
chrome.metricsPrivate = {};

/**
 * Returns true if the user opted in to sending crash reports.
 * @param {Function} callback
 */
chrome.metricsPrivate.getIsCrashReportingEnabled = function(callback) {};

/**
 * Returns the group name chosen for the named trial, or the empty string if
 * the trial does not exist or is not enabled.
 * @param {string} name
 * @param {Function} callback
 */
chrome.metricsPrivate.getFieldTrial = function(name, callback) {};

/**
 * Returns variation parameters for the named trial if available, or undefined
 * otherwise.
 * @param {string} name
 * @param {Function} callback
 */
chrome.metricsPrivate.getVariationParams = function(name, callback) {};

/**
 * Records an action performed by the user.
 * @param {string} name
 */
chrome.metricsPrivate.recordUserAction = function(name) {};

/**
 * Records a percentage value from 1 to 100.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordPercentage = function(metricName, value) {};

/**
 * Records a value than can range from 1 to 1,000,000.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordCount = function(metricName, value) {};

/**
 * Records a value than can range from 1 to 100.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordSmallCount = function(metricName, value) {};

/**
 * Records a value than can range from 1 to 10,000.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordMediumCount = function(metricName, value) {};

/**
 * Records an elapsed time of no more than 10 seconds.  The sample value is
 * specified in milliseconds.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordTime = function(metricName, value) {};

/**
 * Records an elapsed time of no more than 3 minutes.  The sample value is
 * specified in milliseconds.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordMediumTime = function(metricName, value) {};

/**
 * Records an elapsed time of no more than 1 hour.  The sample value is
 * specified in milliseconds.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordLongTime = function(metricName, value) {};

/**
 * Increments the count associated with |value| in the sparse histogram defined
 * by the |metricName|.
 * @param {string} metricName
 * @param {number} value
 */
chrome.metricsPrivate.recordSparseValue = function(metricName, value) {};

/**
 * Adds a value to the given metric.
 * @param {MetricType} metric
 * @param {number} value
 */
chrome.metricsPrivate.recordValue = function(metric, value) {};
