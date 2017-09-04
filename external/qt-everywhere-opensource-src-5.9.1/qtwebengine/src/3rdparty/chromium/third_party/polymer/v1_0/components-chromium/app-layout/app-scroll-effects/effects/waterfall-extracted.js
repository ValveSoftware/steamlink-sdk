/**
   * Toggles the shadow property in app-header when content is scrolled to create a sense of depth
   * between the element and the content underneath.
   */
  Polymer.AppLayout.registerEffect('waterfall', {
    /**
     *  @this Polymer.AppLayout.ElementWithBackground
     */
    run: function run(p, y) {
      this.shadow = this.isOnScreen() && this.isContentBelow();
    }
  });