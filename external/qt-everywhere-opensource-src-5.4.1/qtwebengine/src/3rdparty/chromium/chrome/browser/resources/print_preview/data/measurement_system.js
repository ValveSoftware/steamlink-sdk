// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Measurement system of the print preview. Used to parse and serialize point
   * measurements into the system's local units (e.g. millimeters, inches).
   * @param {string} thousandsDelimeter Delimeter between thousands digits.
   * @param {string} decimalDelimeter Delimeter between integers and decimals.
   * @param {!print_preview.MeasurementSystem.UnitType} unitType Measurement
   *     unit type of the system.
   * @constructor
   */
  function MeasurementSystem(thousandsDelimeter, decimalDelimeter, unitType) {
    this.thousandsDelimeter_ = thousandsDelimeter || ',';
    this.decimalDelimeter_ = decimalDelimeter || '.';
    this.unitType_ = unitType;
  };

  /**
   * Parses |numberFormat| and extracts the symbols used for the thousands point
   * and decimal point.
   * @param {string} numberFormat The formatted version of the number 12345678.
   * @return {!Array.<string>} The extracted symbols in the order
   *     [thousandsSymbol, decimalSymbol]. For example,
   *     parseNumberFormat("123,456.78") returns [",", "."].
   */
  MeasurementSystem.parseNumberFormat = function(numberFormat) {
    if (!numberFormat) {
      return [',', '.'];
    }
    var regex = /^(\d+)(\W?)(\d+)(\W?)(\d+)$/;
    var matches = numberFormat.match(regex) || ['', '', ',', '', '.'];
    return [matches[2], matches[4]];
  };

  /**
   * Enumeration of measurement unit types.
   * @enum {number}
   */
  MeasurementSystem.UnitType = {
    METRIC: 0, // millimeters
    IMPERIAL: 1 // inches
  };

  /**
   * Maximum resolution of local unit values.
   * @type {!Object.<!print_preview.MeasurementSystem.UnitType, number>}
   * @private
   */
  MeasurementSystem.Precision_ = {};
  MeasurementSystem.Precision_[MeasurementSystem.UnitType.METRIC] = 0.5;
  MeasurementSystem.Precision_[MeasurementSystem.UnitType.IMPERIAL] = 0.01;

  /**
   * Maximum number of decimal places to keep for local unit.
   * @type {!Object.<!print_preview.MeasurementSystem.UnitType, number>}
   * @private
   */
  MeasurementSystem.DecimalPlaces_ = {};
  MeasurementSystem.DecimalPlaces_[MeasurementSystem.UnitType.METRIC] = 1;
  MeasurementSystem.DecimalPlaces_[MeasurementSystem.UnitType.IMPERIAL] = 2;

  /**
   * Number of points per inch.
   * @type {number}
   * @const
   * @private
   */
  MeasurementSystem.PTS_PER_INCH_ = 72.0;

  /**
   * Number of points per millimeter.
   * @type {number}
   * @const
   * @private
   */
  MeasurementSystem.PTS_PER_MM_ = MeasurementSystem.PTS_PER_INCH_ / 25.4;

  MeasurementSystem.prototype = {
    /** @return {string} The unit type symbol of the measurement system. */
    get unitSymbol() {
      if (this.unitType_ == MeasurementSystem.UnitType.METRIC) {
        return 'mm';
      } else if (this.unitType_ == MeasurementSystem.UnitType.IMPERIAL) {
        return '"';
      } else {
        throw Error('Unit type not supported: ' + this.unitType_);
      }
    },

    /**
     * @return {string} The thousands delimeter character of the measurement
     *     system.
     */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    },

    /**
     * @return {string} The decimal delimeter character of the measurement
     *     system.
     */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    },

    setSystem: function(thousandsDelimeter, decimalDelimeter, unitType) {
      this.thousandsDelimeter_ = thousandsDelimeter;
      this.decimalDelimeter_ = decimalDelimeter;
      this.unitType_ = unitType;
    },

    /**
     * Rounds a value in the local system's units to the appropriate precision.
     * @param {number} value Value to round.
     * @return {number} Rounded value.
     */
    roundValue: function(value) {
      var precision = MeasurementSystem.Precision_[this.unitType_];
      var roundedValue = Math.round(value / precision) * precision;
      // Truncate
      return roundedValue.toFixed(
          MeasurementSystem.DecimalPlaces_[this.unitType_]);
    },

    /**
     * @param {number} pts Value in points to convert to local units.
     * @return {number} Value in local units.
     */
    convertFromPoints: function(pts) {
      if (this.unitType_ == MeasurementSystem.UnitType.METRIC) {
        return pts / MeasurementSystem.PTS_PER_MM_;
      } else {
        return pts / MeasurementSystem.PTS_PER_INCH_;
      }
    },

    /**
     * @param {number} Value in local units to convert to points.
     * @return {number} Value in points.
     */
    convertToPoints: function(localUnits) {
      if (this.unitType_ == MeasurementSystem.UnitType.METRIC) {
        return localUnits * MeasurementSystem.PTS_PER_MM_;
      } else {
        return localUnits * MeasurementSystem.PTS_PER_INCH_;
      }
    }
  };

  // Export
  return {
    MeasurementSystem: MeasurementSystem
  };
});
