// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * GeolocationOptions class
   * Handles initialization of the geolocation options.
   * @constructor
   * @class
   */
  function GeolocationOptions() {
    OptionsPage.call(this,
                     'geolocationOptions',
                     loadTimeData.getString('geolocationOptionsPageTabTitle'),
                     'geolocationCheckbox');
  };

  cr.addSingletonGetter(GeolocationOptions);

  GeolocationOptions.prototype = {
    __proto__: OptionsPage.prototype
  };

  // TODO(robliao): Determine if a full unroll is necessary
  // (http://crbug.com/306613).
  GeolocationOptions.showGeolocationOption = function() {};

  return {
    GeolocationOptions: GeolocationOptions
  };
});
