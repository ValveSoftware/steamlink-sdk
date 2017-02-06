'use strict';

  Polymer({
    is: 'app-route',

    properties: {
      /**
       * The URL component managed by this element.
       */
      route: {
        type: Object,
        notify: true
      },

      /**
       * The pattern of slash-separated segments to match `path` against.
       *
       * For example the pattern "/foo" will match "/foo" or "/foo/bar"
       * but not "/foobar".
       *
       * Path segments like `/:named` are mapped to properties on the `data` object.
       */
      pattern: {
        type: String
      },

      /**
       * The parameterized values that are extracted from the route as
       * described by `pattern`.
       */
      data: {
        type: Object,
        value: function() {return {};},
        notify: true
      },

      /**
       * @type {?Object}
       */
      queryParams: {
        type: Object,
        value: function() {
          return {};
        },
        notify: true
      },

      /**
       * The part of `path` NOT consumed by `pattern`.
       */
      tail: {
        type: Object,
        value: function() {return {path: null, prefix: null, __queryParams: null};},
        notify: true
      },

      active: {
        type: Boolean,
        notify: true,
        readOnly: true
      },

      _queryParamsUpdating: {
        type: Boolean,
        value: false
      },
      /**
       * @type {?string}
       */
      _matched: {
        type: String,
        value: ''
      }
    },

    observers: [
      '__tryToMatch(route.path, pattern)',
      '__updatePathOnDataChange(data.*)',
      '__tailPathChanged(tail.path)',
      '__routeQueryParamsChanged(route.__queryParams)',
      '__tailQueryParamsChanged(tail.__queryParams)',
      '__queryParamsChanged(queryParams.*)'
    ],

    created: function() {
      this.linkPaths('route.__queryParams', 'tail.__queryParams');
      this.linkPaths('tail.__queryParams', 'route.__queryParams');
    },

    /**
     * Deal with the query params object being assigned to wholesale.
     * @export
     */
    __routeQueryParamsChanged: function(queryParams) {
      if (queryParams && this.tail) {
        this.set('tail.__queryParams', queryParams);

        if (!this.active || this._queryParamsUpdating) {
          return;
        }

        // Copy queryParams and track whether there are any differences compared
        // to the existing query params.
        var copyOfQueryParams = {};
        var anythingChanged = false;
        for (var key in queryParams) {
          copyOfQueryParams[key] = queryParams[key];
          if (anythingChanged ||
              !this.queryParams ||
              queryParams[key] !== this.queryParams[key]) {
            anythingChanged = true;
          }
        }
        // Need to check whether any keys were deleted
        for (var key in this.queryParams) {
          if (anythingChanged || !(key in queryParams)) {
            anythingChanged = true;
            break;
          }
        }

        if (!anythingChanged) {
          return;
        }
        this._queryParamsUpdating = true;
        this.set('queryParams', copyOfQueryParams);
        this._queryParamsUpdating = false;
      }
    },

    /**
     * @export
     */
    __tailQueryParamsChanged: function(queryParams) {
      if (queryParams && this.route) {
        this.set('route.__queryParams', queryParams);
      }
    },

    /**
     * @export
     */
    __queryParamsChanged: function(changes) {
      if (!this.active || this._queryParamsUpdating) {
        return;
      }

      this.set('route.__' + changes.path, changes.value);
    },

    __resetProperties: function() {
      this._setActive(false);
      this._matched = null;
      //this.tail = { path: null, prefix: null, queryParams: null };
      //this.data = {};
    },

    /**
     * @export
     */
    __tryToMatch: function() {
      if (!this.route) {
        return;
      }
      var path = this.route.path;
      var pattern = this.pattern;
      if (!pattern) {
        return;
      }

      if (!path) {
        this.__resetProperties();
        return;
      }

      var remainingPieces = path.split('/');
      var patternPieces = pattern.split('/');

      var matched = [];
      var namedMatches = {};

      for (var i=0; i < patternPieces.length; i++) {
        var patternPiece = patternPieces[i];
        if (!patternPiece && patternPiece !== '') {
          break;
        }
        var pathPiece = remainingPieces.shift();

        // We don't match this path.
        if (!pathPiece && pathPiece !== '') {
          this.__resetProperties();
          return;
        }
        matched.push(pathPiece);

        if (patternPiece.charAt(0) == ':') {
          namedMatches[patternPiece.slice(1)] = pathPiece;
        } else if (patternPiece !== pathPiece) {
          this.__resetProperties();
          return;
        }
      }

      this._matched = matched.join('/');

      // Properties that must be updated atomically.
      var propertyUpdates = {};

      //this.active
      if (!this.active) {
        propertyUpdates.active = true;
      }

      // this.tail
      var tailPrefix = this.route.prefix + this._matched;
      var tailPath = remainingPieces.join('/');
      if (remainingPieces.length > 0) {
        tailPath = '/' + tailPath;
      }
      if (!this.tail ||
          this.tail.prefix !== tailPrefix ||
          this.tail.path !== tailPath) {
        propertyUpdates.tail = {
          prefix: tailPrefix,
          path: tailPath,
          __queryParams: this.route.__queryParams
        };
      }

      // this.data
      propertyUpdates.data = namedMatches;
      this._dataInUrl = {};
      for (var key in namedMatches) {
        this._dataInUrl[key] = namedMatches[key];
      }

      this.__setMulti(propertyUpdates);
    },

    /**
     * @export
     */
    __tailPathChanged: function() {
      if (!this.active) {
        return;
      }
      var tailPath = this.tail.path;
      var newPath = this._matched;
      if (tailPath) {
        if (tailPath.charAt(0) !== '/') {
          tailPath = '/' + tailPath;
        }
        newPath += tailPath;
      }
      this.set('route.path', newPath);
    },

    /**
     * @export
     */
    __updatePathOnDataChange: function() {
      if (!this.route || !this.active) {
        return;
      }
      var newPath = this.__getLink({});
      var oldPath = this.__getLink(this._dataInUrl);
      if (newPath === oldPath) {
        return;
      }
      this.set('route.path', newPath);
    },

    __getLink: function(overrideValues) {
      var values = {tail: null};
      for (var key in this.data) {
        values[key] = this.data[key];
      }
      for (var key in overrideValues) {
        values[key] = overrideValues[key];
      }
      var patternPieces = this.pattern.split('/');
      var interp = patternPieces.map(function(value) {
        if (value[0] == ':') {
          value = values[value.slice(1)];
        }
        return value;
      }, this);
      if (values.tail && values.tail.path) {
        if (interp.length > 0 && values.tail.path.charAt(0) === '/') {
          interp.push(values.tail.path.slice(1));
        } else {
          interp.push(values.tail.path);
        }
      }
      return interp.join('/');
    },

    __setMulti: function(setObj) {
      // HACK(rictic): skirting around 1.0's lack of a setMulti by poking at
      //     internal data structures. I would not advise that you copy this
      //     example.
      //
      //     In the future this will be a feature of Polymer itself.
      //     See: https://github.com/Polymer/polymer/issues/3640
      //
      //     Hacking around with private methods like this is juggling footguns,
      //     and is likely to have unexpected and unsupported rough edges.
      //
      //     Be ye so warned.
      for (var property in setObj) {
        this._propertySetter(property, setObj[property]);
      }

      for (var property in setObj) {
        this._pathEffector(property, this[property]);
        this._notifyPathUp(property, this[property]);
      }
    }
  });