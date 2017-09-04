Polymer({
      is: 'app-scrollpos-control',

      properties: {
        /**
         * The selected page.
         */
        selected: {
          type: String,
          observer: '_selectedChanged'
        },

        /**
         * Reset the scroll position to 0.
         */
        reset: {
          type: Boolean,
          value: false
        }
      },

      observers: [
        '_updateScrollPos(selected, reset)'
      ],

      created: function() {
        this._scrollposMap = {};
      },

      _selectedChanged: function(selected, old) {
        if (old != null) {
          this._scrollposMap[old] = {x: window.pageXOffset, y: window.pageYOffset};
        }
      },

      _updateScrollPos: function(selected, reset) {
        this.debounce('_updateScrollPos', function() {
          var pos = this._scrollposMap[this.selected];
          if (pos != null && !this.reset) {
            this._scrollTo(pos.x, pos.y);
          } else {
            this._scrollTo(0, 0);
          }
        });
      },

      _scrollTo: function(x, y) {
        Polymer.AppLayout.scroll({
          left: x,
          top: y,
          behavior: 'silent'
        });
      }
    });