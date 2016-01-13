// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */

/**
 * Takes the |experimentsData| input argument which represents data about all
 * the current experiments and populates the html jstemplate with that data.
 * with that data. It expects an object structure like the above.
 * @param {Object} experimentsData Information about all experiments.
 *     See returnFlagsExperiments() for the structure of this object.
 */
function renderTemplate(experimentsData) {
  // This is the javascript code that processes the template:
  jstProcess(new JsEvalContext(experimentsData),
             $('flagsExperimentTemplate'));

  // Add handlers to dynamically created HTML elements.
  var elements = document.getElementsByClassName('experiment-select');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onchange = function() {
      handleSelectChoiceExperiment(this, this.selectedIndex);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-disable-link');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = function() {
      handleEnableExperiment(this, false);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-enable-link');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = function() {
      handleEnableExperiment(this, true);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-restart-button');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = restartBrowser;
  }

  $('experiment-reset-all').onclick = resetAllFlags;

  highlightReferencedFlag();
}

/**
 * Highlight an element associated with the page's location's hash. We need to
 * fake fragment navigation with '.scrollIntoView()', since the fragment IDs
 * don't actually exist until after the template code runs; normal navigation
 * therefore doesn't work.
 */
function highlightReferencedFlag() {
  if (window.location.hash) {
    var el = document.querySelector(window.location.hash);
    if (el && !el.classList.contains('referenced')) {
      // Unhighlight whatever's highlighted.
      if (document.querySelector('.referenced'))
        document.querySelector('.referenced').classList.remove('referenced');
      // Highlight the referenced element.
      el.classList.add('referenced');
      el.scrollIntoView();
    }
  }
}

/**
 * Asks the C++ FlagsDOMHandler to get details about the available experiments
 * and return detailed data about the configuration. The FlagsDOMHandler
 * should reply to returnFlagsExperiments() (below).
 */
function requestFlagsExperimentsData() {
  chrome.send('requestFlagsExperiments');
}

/**
 * Asks the C++ FlagsDOMHandler to restart the browser (restoring tabs).
 */
function restartBrowser() {
  chrome.send('restartBrowser');
}

/**
 * Reset all flags to their default values and refresh the UI.
 */
function resetAllFlags() {
  // Asks the C++ FlagsDOMHandler to reset all flags to default values.
  chrome.send('resetAllFlags');
  requestFlagsExperimentsData();
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of all experiments.
 * @param {Object} experimentsData Information about all experiments.
 *     in the following format:
 *   {
 *     supportedExperiments: [
 *       {
 *         internal_name: 'Experiment ID string',
 *         name: 'Experiment Name',
 *         description: 'description',
 *         // enabled is only set if the experiment is single valued.
 *         enabled: true,
 *         // choices is only set if the experiment has multiple values.
 *         choices: [
 *           {
 *             internal_name: 'Experiment ID string',
 *             description: 'description',
 *             selected: true
 *           }
 *         ],
 *         supported_platforms: [
 *           'Mac',
 *           'Linux'
 *         ],
 *       }
 *     ],
 *     unsupportedExperiments: [
 *       // Mirrors the format of |supportedExperiments| above.
 *     ],
 *     needsRestart: false,
 *     showBetaChannelPromotion: false,
 *     showDevChannelPromotion: false,
 *     showOwnerWarning: false
 *   }
 */
function returnFlagsExperiments(experimentsData) {
  var bodyContainer = $('body-container');
  renderTemplate(experimentsData);

  if (experimentsData.showBetaChannelPromotion)
    $('channel-promo-beta').hidden = false;
  else if (experimentsData.showDevChannelPromotion)
    $('channel-promo-dev').hidden = false;

  bodyContainer.style.visibility = 'visible';
  var ownerWarningDiv = $('owner-warning');
  if (ownerWarningDiv)
    ownerWarningDiv.hidden = !experimentsData.showOwnerWarning;
}

/**
 * Handles a 'enable' or 'disable' button getting clicked.
 * @param {HTMLElement} node The node for the experiment being changed.
 * @param {boolean} enable Whether to enable or disable the experiment.
 */
function handleEnableExperiment(node, enable) {
  // Tell the C++ FlagsDOMHandler to enable/disable the experiment.
  chrome.send('enableFlagsExperiment', [String(node.internal_name),
                                        String(enable)]);
  requestFlagsExperimentsData();
}

/**
 * Invoked when the selection of a multi-value choice is changed to the
 * specified index.
 * @param {HTMLElement} node The node for the experiment being changed.
 * @param {number} index The index of the option that was selected.
 */
function handleSelectChoiceExperiment(node, index) {
  // Tell the C++ FlagsDOMHandler to enable the selected choice.
  chrome.send('enableFlagsExperiment',
              [String(node.internal_name) + '@' + index, 'true']);
  requestFlagsExperimentsData();
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestFlagsExperimentsData);

// Update the highlighted flag when the hash changes.
window.addEventListener('hashchange', highlightReferencedFlag);
