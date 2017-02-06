// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.login', function() {
  /**
   * Constructs a slide manager for the user manager tutorial.
   *
   * @constructor
   */
  function UserManagerTutorial() {
  }

  cr.addSingletonGetter(UserManagerTutorial);

  UserManagerTutorial.prototype = {
    /**
     * Tutorial slides.
     */
    slides_: ['slide-your-chrome',
              'slide-friends',
              'slide-guests',
              'slide-complete',
              'slide-not-you'],

    /**
     * Current tutorial step, index in the slides array.
     * @type {number}
     */
    currentStep_: 0,

    /**
     * Switches to the next tutorial step.
     * @param {number} nextStepIndex Index of the next step.
     */
    toggleStep_: function(nextStepIndex) {
      if (nextStepIndex > this.slides_.length)
        return;

      var currentStepId = this.slides_[this.currentStep_];
      var nextStepId = this.slides_[nextStepIndex];
      var oldStep = $(currentStepId);
      var newStep = $(nextStepId);

      newStep.classList.remove('hidden');

      if (nextStepIndex != this.currentStep_)
        oldStep.classList.add('hidden');

      this.currentStep_ = nextStepIndex;

      // The last tutorial step is an information bubble that ends the tutorial.
      if (this.currentStep_ == this.slides_.length - 1)
        this.endTutorial_();
    },

    next_: function() {
      var nextStep = this.currentStep_ + 1;
      this.toggleStep_(nextStep);
    },

    skip_: function() {
      this.endTutorial_();
      $('user-manager-tutorial').hidden = true;
    },

    /**
     * Add user button click handler.
     * @private
     */
    handleAddUserClick_: function(e) {
      chrome.send('addUser');
      $('user-manager-tutorial').hidden = true;
      e.stopPropagation();
    },

    /**
     * Add a button click handler to dismiss the last tutorial bubble.
     * @private
     */
    handleDismissBubbleClick_: function(e) {
      $('user-manager-tutorial').hidden = true;
      e.stopPropagation();
    },

    endTutorial_: function(e) {
      $('inner-container').classList.remove('disabled');
    },

    decorate: function() {
      // Transitions between the tutorial slides.
      for (var i = 0; i < this.slides_.length; ++i) {
        var buttonNext = $(this.slides_[i] + '-next');
        var buttonSkip = $(this.slides_[i] + '-skip');
        if (buttonNext)
          buttonNext.addEventListener('click', this.next_.bind(this));
        if (buttonSkip)
          buttonSkip.addEventListener('click', this.skip_.bind(this));
      }
      $('slide-add-user').addEventListener('click',
          this.handleAddUserClick_.bind(this));
      $('dismiss-bubble-button').addEventListener('click',
          this.handleDismissBubbleClick_.bind(this));
    }
  };

  /**
   * Initializes the tutorial manager.
   */
  UserManagerTutorial.startTutorial = function() {
    $('user-manager-tutorial').hidden = false;

    // If there's only one pod, show the slides to the side of the pod.
    // Otherwise, center the slides and disable interacting with the pods
    // while the tutorial is showing.
    if ($('pod-row').pods.length == 1) {
      $('slide-your-chrome').classList.add('single-pod');
      $('slide-complete').classList.add('single-pod');
    }

    $('pod-row').focusPod();  // No focused pods.
    $('inner-container').classList.add('disabled');
  };

  /**
   * Initializes the tutorial manager.
   */
  UserManagerTutorial.initialize = function() {
    UserManagerTutorial.getInstance().decorate();
  };

  // Export.
  return {
    UserManagerTutorial: UserManagerTutorial
  };
});
