/**
   * Shorthand for the waterfall, resize-title, blend-background, and parallax-background effects.
   */
  Polymer.AppLayout.registerEffect('material', {
    /**
     * @this Polymer.AppLayout.ElementWithBackground
     */
    setUp: function setUp() {
      this.effects = 'waterfall resize-title blend-background parallax-background';
      return false;
    }
  });