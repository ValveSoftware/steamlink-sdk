Polymer({
      is: 'app-box',

      behaviors: [
        Polymer.AppScrollEffectsBehavior,
        Polymer.IronResizableBehavior
      ],

      listeners: {
        'iron-resize': '_resizeHandler'
      },

      /**
       * The current scroll progress.
       *
       * @type {number}
       */
      _progress: 0,

      attached: function() {
        this.resetLayout();
      },

      /**
       * Resets the layout. This method is automatically called when the element is attached to the DOM.
       *
       * @method resetLayout
       */
      resetLayout: function() {
        this.debounce('_resetLayout', function() {
          // noop if the box isn't in the rendered tree
          if (this.offsetWidth === 0 && this.offsetHeight === 0) {
            return;
          }

          var scrollTop = this._clampedScrollTop;
          var savedDisabled = this.disabled;

          this.disabled = true;
          this._elementTop = this._getElementTop();
          this._elementHeight = this.offsetHeight;
          this._cachedScrollTargetHeight = this._scrollTargetHeight;
          this._setUpEffect();
          this._updateScrollState(scrollTop);
          this.disabled = savedDisabled;
        }, 1);
      },

      _getElementTop: function() {
        var currentNode = this;
        var top = 0;

        while (currentNode && currentNode !== this.scrollTarget) {
          top += currentNode.offsetTop;
          currentNode = currentNode.offsetParent;
        }
        return top;
      },

      _updateScrollState: function(scrollTop) {
        if (this.isOnScreen()) {
          var viewportTop = this._elementTop - scrollTop;
          this._progress = 1 - (viewportTop + this._elementHeight) / this._cachedScrollTargetHeight;
          this._runEffects(this._progress, scrollTop);
        }
      },

      /**
       * Returns true if this app-box is on the screen.
       * That is, visible in the current viewport.
       *
       * @method isOnScreen
       * @return {boolean}
       */
      isOnScreen: function() {
        return this._elementTop < this._scrollTop + this._cachedScrollTargetHeight
            && this._elementTop + this._elementHeight > this._scrollTop;
      },

      _resizeHandler: function() {
        this.resetLayout();
      },

      _getDOMRef: function(id) {
        if (id === 'background') {
          return this.$.background;
        }
        if (id === 'backgroundFrontLayer') {
          return this.$.backgroundFrontLayer;
        }
      },

      /**
       * Returns an object containing the progress value of the scroll effects.
       *
       * @method getScrollState
       * @return {Object}
       */
      getScrollState: function() {
        return { progress: this._progress };
      }
    });