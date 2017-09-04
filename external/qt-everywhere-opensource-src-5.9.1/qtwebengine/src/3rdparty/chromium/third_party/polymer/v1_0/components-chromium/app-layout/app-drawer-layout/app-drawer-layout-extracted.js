Polymer({
      is: 'app-drawer-layout',

      behaviors: [
        Polymer.IronResizableBehavior
      ],

      properties: {
        /**
         * If true, ignore `responsiveWidth` setting and force the narrow layout.
         */
        forceNarrow: {
          type: Boolean,
          value: false
        },

        /**
         * If the viewport's width is smaller than this value, the panel will change to narrow
         * layout. In the mode the drawer will be closed.
         */
        responsiveWidth: {
          type: String,
          value: '640px'
        },

        /**
         * Returns true if it is in narrow layout. This is useful if you need to show/hide
         * elements based on the layout.
         */
        narrow: {
          type: Boolean,
          readOnly: true,
          notify: true
        }
      },

      listeners: {
        'tap': '_tapHandler',
        'app-drawer-reset-layout': 'resetLayout'
      },

      observers: [
        'resetLayout(narrow, isAttached)'
      ],

      /**
       * A reference to the app-drawer element.
       *
       * @property drawer
       */
      get drawer() {
        return Polymer.dom(this.$.drawerContent).getDistributedNodes()[0];
      },

      _tapHandler: function(e) {
        var target = Polymer.dom(e).localTarget;
        if (target && target.hasAttribute('drawer-toggle')) {
          this.drawer.toggle();
        }
      },

      resetLayout: function() {
        this.debounce('_resetLayout', function() {
          if (!this.isAttached) {
            return;
          }

          var drawer = this.drawer;
          var drawerWidth = this.drawer.getWidth();
          var contentContainer = this.$.contentContainer;

          if (this.narrow) {
            drawer.opened = drawer.persistent = false;
            contentContainer.classList.add('narrow');

            contentContainer.style.marginLeft = '';
            contentContainer.style.marginRight = '';
          } else {
            drawer.opened = drawer.persistent = true;
            contentContainer.classList.remove('narrow');

            if (drawer.position == 'right') {
              contentContainer.style.marginLeft = '';
              contentContainer.style.marginRight = drawerWidth + 'px';
            } else {
              contentContainer.style.marginLeft = drawerWidth + 'px';
              contentContainer.style.marginRight = '';
            }
          }

          this.notifyResize();
        });
      },

      _onQueryMatchesChanged: function(event) {
        this._setNarrow(event.detail.value);
      },

      _computeMediaQuery: function(forceNarrow, responsiveWidth) {
        return forceNarrow ? '(min-width: 0px)' : '(max-width: ' + responsiveWidth + ')';
      }
    });