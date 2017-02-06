/**
   * `Polymer.NeonAnimationRunnerBehavior` adds a method to run animations.
   *
   * @polymerBehavior Polymer.NeonAnimationRunnerBehavior
   */
  Polymer.NeonAnimationRunnerBehaviorImpl = {

    properties: {

      /** @type {?Object} */
      _player: {
        type: Object
      }

    },

    _configureAnimationEffects: function(allConfigs) {
      var allAnimations = [];
      if (allConfigs.length > 0) {
        for (var config, index = 0; config = allConfigs[index]; index++) {
          var animation = document.createElement(config.name);
          // is this element actually a neon animation?
          if (animation.isNeonAnimation) {
            var effect = animation.configure(config);
            if (effect) {
              allAnimations.push({
                animation: animation,
                config: config,
                effect: effect
              });
            }
          } else {
            console.warn(this.is + ':', config.name, 'not found!');
          }
        }
      }
      return allAnimations;
    },

    _runAnimationEffects: function(allEffects) {
      return document.timeline.play(new GroupEffect(allEffects));
    },

    _completeAnimations: function(allAnimations) {
      for (var animation, index = 0; animation = allAnimations[index]; index++) {
        animation.animation.complete(animation.config);
      }
    },

    /**
     * Plays an animation with an optional `type`.
     * @param {string=} type
     * @param {!Object=} cookie
     */
    playAnimation: function(type, cookie) {
      var allConfigs = this.getAnimationConfig(type);
      if (!allConfigs) {
        return;
      }
      try {
        var allAnimations = this._configureAnimationEffects(allConfigs);
        var allEffects = allAnimations.map(function(animation) {
          return animation.effect;
        });

        if (allEffects.length > 0) {
          this._player = this._runAnimationEffects(allEffects);
          this._player.onfinish = function() {
            this._completeAnimations(allAnimations);

            if (this._player) {
              this._player.cancel();
              this._player = null;
            }

            this.fire('neon-animation-finish', cookie, {bubbles: false});
          }.bind(this);
          return;
        }
      } catch (e) {
        console.warn('Couldnt play', '(', type, allConfigs, ').', e);
      }
      this.fire('neon-animation-finish', cookie, {bubbles: false});
    },

    /**
     * Cancels the currently running animation.
     */
    cancelAnimation: function() {
      if (this._player) {
        this._player.cancel();
      }
    }
  };

  /** @polymerBehavior Polymer.NeonAnimationRunnerBehavior */
  Polymer.NeonAnimationRunnerBehavior = [
    Polymer.NeonAnimatableBehavior,
    Polymer.NeonAnimationRunnerBehaviorImpl
  ];