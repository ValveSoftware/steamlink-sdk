// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counter used to give webkit animations unique names.
var animationCounter = 0;

function addAnimation(code) {
  var name = 'anim' + animationCounter;
  animationCounter++;
  var rules = document.createTextNode(
      '@-webkit-keyframes ' + name + ' {' + code + '}');
  var el = document.createElement('style');
  el.type = 'text/css';
  el.appendChild(rules);
  el.setAttribute('id', name);
  document.body.appendChild(el);

  return name;
}

/**
 * Generates css code for fading in an element by animating the height.
 * @param {number} targetHeight The desired height in pixels after the animation
 *     ends.
 * @return {string} The css code for the fade in animation.
 */
function getFadeInAnimationCode(targetHeight) {
  return '0% { opacity: 0; height: 0; } ' +
      '80% { opacity: 0.5; height: ' + (targetHeight + 4) + 'px; }' +
      '100% { opacity: 1; height: ' + targetHeight + 'px; }';
}

/**
 * Fades in an element. Used for both printing options and error messages
 * appearing underneath the textfields.
 * @param {HTMLElement} el The element to be faded in.
 */
function fadeInElement(el) {
  if (el.classList.contains('visible'))
    return;
  el.classList.remove('closing');
  el.hidden = false;
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  var animName = addAnimation(getFadeInAnimationCode(height));
  var eventTracker = new EventTracker();
  eventTracker.add(el, 'webkitAnimationEnd',
                   onFadeInAnimationEnd.bind(el, eventTracker),
                   false);
  el.style.webkitAnimationName = animName;
  el.classList.add('visible');
}

/**
 * Fades out an element. Used for both printing options and error messages
 * appearing underneath the textfields.
 * @param {HTMLElement} el The element to be faded out.
 */
function fadeOutElement(el) {
  if (!el.classList.contains('visible'))
    return;
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  el.offsetHeight;  // Should force an update of the computed style.
  var eventTracker = new EventTracker();
  eventTracker.add(el, 'webkitTransitionEnd',
                   onFadeOutTransitionEnd.bind(el, eventTracker),
                   false);
  el.classList.add('closing');
  el.classList.remove('visible');
}

/**
 * Executes when a fade out animation ends.
 * @param {EventTracker} eventTracker The |EventTracker| object that was used
 *     for adding this listener.
 * @param {WebkitTransitionEvent} event The event that triggered this listener.
 * @this {HTMLElement} The element where the transition occurred.
 */
function onFadeOutTransitionEnd(eventTracker, event) {
  if (event.propertyName != 'height')
    return;
  eventTracker.remove(this, 'webkitTransitionEnd');
  this.hidden = true;
}

/**
 * Executes when a fade in animation ends.
 * @param {EventTracker} eventTracker The |EventTracker| object that was used
 *     for adding this listener.
 * @param {WebkitAnimationEvent} event The event that triggered this listener.
 * @this {HTMLElement} The element where the transition occurred.
 */
function onFadeInAnimationEnd(eventTracker, event) {
  this.style.height = '';
  this.style.webkitAnimationName = '';
  fadeInOutCleanup(event.animationName);
  eventTracker.remove(this, 'webkitAnimationEnd');
}

/**
 * Removes the <style> element corrsponding to |animationName| from the DOM.
 * @param {string} animationName The name of the animation to be removed.
 */
function fadeInOutCleanup(animationName) {
  var animEl = document.getElementById(animationName);
  if (animEl)
    animEl.parentNode.removeChild(animEl);
}

/**
 * Fades in a printing option existing under |el|.
 * @param {HTMLElement} el The element to hide.
 */
function fadeInOption(el) {
  if (el.classList.contains('visible'))
    return;
  // To make the option visible during the first fade in.
  el.hidden = false;

  wrapContentsInDiv(el.querySelector('h1'), ['invisible']);
  var rightColumn = el.querySelector('.right-column');
  wrapContentsInDiv(rightColumn, ['invisible']);

  var toAnimate = el.querySelectorAll('.collapsible');
  for (var i = 0; i < toAnimate.length; i++)
    fadeInElement(toAnimate[i]);
  el.classList.add('visible');
}

/**
 * Fades out a printing option existing under |el|.
 * @param {HTMLElement} el The element to hide.
 * @param {boolean=} opt_justHide Whether {@code el} should be hidden with no
 *     animation.
 */
function fadeOutOption(el, opt_justHide) {
  if (!el.classList.contains('visible'))
    return;

  wrapContentsInDiv(el.querySelector('h1'), ['visible']);
  var rightColumn = el.querySelector('.right-column');
  wrapContentsInDiv(rightColumn, ['visible']);

  var toAnimate = el.querySelectorAll('.collapsible');
  for (var i = 0; i < toAnimate.length; i++) {
    if (opt_justHide) {
      toAnimate[i].hidden = true;
    } else {
      fadeOutElement(toAnimate[i]);
    }
  }
  el.classList.remove('visible');
}

/**
 * Wraps the contents of |el| in a div element and attaches css classes
 * |classes| in the new div, only if has not been already done. It is neccesary
 * for animating the height of table cells.
 * @param {HTMLElement} el The element to be processed.
 * @param {array} classes The css classes to add.
 */
function wrapContentsInDiv(el, classes) {
  var div = el.querySelector('div');
  if (!div || !div.classList.contains('collapsible')) {
    div = document.createElement('div');
    while (el.childNodes.length > 0)
      div.appendChild(el.firstChild);
    el.appendChild(div);
  }

  div.className = '';
  div.classList.add('collapsible');
  for (var i = 0; i < classes.length; i++)
    div.classList.add(classes[i]);
}
