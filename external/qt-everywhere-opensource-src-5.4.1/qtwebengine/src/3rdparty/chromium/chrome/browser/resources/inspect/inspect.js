// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MIN_VERSION_TAB_CLOSE = 25;
var MIN_VERSION_TARGET_ID = 26;
var MIN_VERSION_NEW_TAB = 29;
var MIN_VERSION_TAB_ACTIVATE = 30;

var queryParamsObject = {};

(function() {
var queryParams = window.location.search;
if (!queryParams)
    return;
var params = queryParams.substring(1).split('&');
for (var i = 0; i < params.length; ++i) {
    var pair = params[i].split('=');
    queryParamsObject[pair[0]] = pair[1];
}

})();

function sendCommand(command, args) {
  chrome.send(command, Array.prototype.slice.call(arguments, 1));
}

function sendTargetCommand(command, target) {
  sendCommand(command, target.source, target.id);
}

function sendServiceWorkerCommand(action, worker) {
  $('serviceworker-internals').contentWindow.postMessage({
    'action': action,
    'worker': worker
  },'chrome://serviceworker-internals');
}

function removeChildren(element_id) {
  var element = $(element_id);
  element.textContent = '';
}

function onload() {
  var tabContents = document.querySelectorAll('#content > div');
  for (var i = 0; i != tabContents.length; i++) {
    var tabContent = tabContents[i];
    var tabName = tabContent.querySelector('.content-header').textContent;

    var tabHeader = document.createElement('div');
    tabHeader.className = 'tab-header';
    var button = document.createElement('button');
    button.textContent = tabName;
    tabHeader.appendChild(button);
    tabHeader.addEventListener('click', selectTab.bind(null, tabContent.id));
    $('navigation').appendChild(tabHeader);
  }
  onHashChange();
  initSettings();
  sendCommand('init-ui');
  window.addEventListener('message', onMessage.bind(this), false);
}

function onMessage(event) {
  if (event.origin != 'chrome://serviceworker-internals') {
    return;
  }
  populateServiceWorkers(event.data.partition_id,
                         event.data.workers);
}

function onHashChange() {
  var hash = window.location.hash.slice(1).toLowerCase();
  if (!selectTab(hash))
    selectTab('devices');
}

/**
 * @param {string} id Tab id.
 * @return {boolean} True if successful.
 */
function selectTab(id) {
  closePortForwardingConfig();

  var tabContents = document.querySelectorAll('#content > div');
  var tabHeaders = $('navigation').querySelectorAll('.tab-header');
  var found = false;
  for (var i = 0; i != tabContents.length; i++) {
    var tabContent = tabContents[i];
    var tabHeader = tabHeaders[i];
    if (tabContent.id == id) {
      tabContent.classList.add('selected');
      tabHeader.classList.add('selected');
      found = true;
    } else {
      tabContent.classList.remove('selected');
      tabHeader.classList.remove('selected');
    }
  }
  if (!found)
    return false;
  window.location.hash = id;
  return true;
}

function populateServiceWorkers(partition_id, workers) {
  var list = $('service-workers-list-' + partition_id);
  if (workers.length == 0) {
    if (list) {
        list.parentNode.removeChild(list);
    }
    return;
  }
  if (list) {
    list.textContent = '';
  } else {
    list = document.createElement('div');
    list.id = 'service-workers-list-' + partition_id;
    list.className = 'list';
    $('service-workers-list').appendChild(list);
  }
  for (var i = 0; i < workers.length; i++) {
    var worker = workers[i];
    worker.hasCustomInspectAction = true;
    var row = addTargetToList(worker, list, ['scope', 'url']);
    addActionLink(
        row,
        'inspect',
        sendServiceWorkerCommand.bind(null, 'inspect', worker),
        false);
    addActionLink(
        row,
        'terminate',
        sendServiceWorkerCommand.bind(null, 'stop', worker),
        false);
  }
}

function populateTargets(source, data) {
  if (source == 'renderers')
    populateWebContentsTargets(data);
  else if (source == 'workers')
    populateWorkerTargets(data);
  else if (source == 'adb')
    populateRemoteTargets(data);
  else
    console.error('Unknown source type: ' + source);
}

function populateWebContentsTargets(data) {
  removeChildren('pages-list');
  removeChildren('extensions-list');
  removeChildren('apps-list');
  removeChildren('others-list');

  for (var i = 0; i < data.length; i++) {
    if (data[i].type === 'page')
      addToPagesList(data[i]);
    else if (data[i].type === 'background_page')
      addToExtensionsList(data[i]);
    else if (data[i].type === 'app')
      addToAppsList(data[i]);
    else
      addToOthersList(data[i]);
  }
}

function populateWorkerTargets(data) {
  removeChildren('workers-list');

  for (var i = 0; i < data.length; i++)
    addToWorkersList(data[i]);
}

function showIncognitoWarning() {
  $('devices-incognito').hidden = false;
}

function populateRemoteTargets(devices) {
  if (!devices)
    return;

  if (window.modal) {
    window.holdDevices = devices;
    return;
  }

  function alreadyDisplayed(element, data) {
    var json = JSON.stringify(data);
    if (element.cachedJSON == json)
      return true;
    element.cachedJSON = json;
    return false;
  }

  function insertChildSortedById(parent, child) {
    for (var sibling = parent.firstElementChild;
                     sibling;
                     sibling = sibling.nextElementSibling) {
      if (sibling.id > child.id) {
        parent.insertBefore(child, sibling);
        return;
      }
    }
    parent.appendChild(child);
  }

  var deviceList = $('devices-list');
  if (alreadyDisplayed(deviceList, devices))
    return;

  function removeObsolete(validIds, section) {
    if (validIds.indexOf(section.id) < 0)
      section.remove();
  }

  var newDeviceIds = devices.map(function(d) { return d.id });
  Array.prototype.forEach.call(
      deviceList.querySelectorAll('.device'),
      removeObsolete.bind(null, newDeviceIds));

  $('devices-help').hidden = !!devices.length;

  for (var d = 0; d < devices.length; d++) {
    var device = devices[d];

    var deviceSection = $(device.id);
    if (!deviceSection) {
      deviceSection = document.createElement('div');
      deviceSection.id = device.id;
      deviceSection.className = 'device';
      deviceList.appendChild(deviceSection);

      var deviceHeader = document.createElement('div');
      deviceHeader.className = 'device-header';
      deviceSection.appendChild(deviceHeader);

      var deviceName = document.createElement('div');
      deviceName.className = 'device-name';
      deviceHeader.appendChild(deviceName);

      var deviceSerial = document.createElement('div');
      deviceSerial.className = 'device-serial';
      deviceSerial.textContent = '#' + device.adbSerial.toUpperCase();
      deviceHeader.appendChild(deviceSerial);

      var devicePorts = document.createElement('div');
      devicePorts.className = 'device-ports';
      deviceHeader.appendChild(devicePorts);

      var browserList = document.createElement('div');
      browserList.className = 'browsers';
      deviceSection.appendChild(browserList);

      var authenticating = document.createElement('div');
      authenticating.className = 'device-auth';
      deviceSection.appendChild(authenticating);
    }

    if (alreadyDisplayed(deviceSection, device))
      continue;

    deviceSection.querySelector('.device-name').textContent = device.adbModel;
    deviceSection.querySelector('.device-auth').textContent =
        device.adbConnected ? '' : 'Pending authentication: please accept ' +
          'debugging session on the device.';

    var browserList = deviceSection.querySelector('.browsers');
    var newBrowserIds =
        device.browsers.map(function(b) { return b.id });
    Array.prototype.forEach.call(
        browserList.querySelectorAll('.browser'),
        removeObsolete.bind(null, newBrowserIds));

    for (var b = 0; b < device.browsers.length; b++) {
      var browser = device.browsers[b];

      var majorChromeVersion = browser.adbBrowserChromeVersion;

      var incompatibleVersion = browser.hasOwnProperty('compatibleVersion') &&
                                !browser.compatibleVersion;
      var pageList;
      var browserSection = $(browser.id);
      if (browserSection) {
        pageList = browserSection.querySelector('.pages');
      } else {
        browserSection = document.createElement('div');
        browserSection.id = browser.id;
        browserSection.className = 'browser';
        insertChildSortedById(browserList, browserSection);

        var browserHeader = document.createElement('div');
        browserHeader.className = 'browser-header';

        var browserName = document.createElement('div');
        browserName.className = 'browser-name';
        browserHeader.appendChild(browserName);
        browserName.textContent = browser.adbBrowserName;
        if (browser.adbBrowserVersion)
          browserName.textContent += ' (' + browser.adbBrowserVersion + ')';
        browserSection.appendChild(browserHeader);

        if (incompatibleVersion) {
          var warningSection = document.createElement('div');
          warningSection.className = 'warning';
          warningSection.textContent =
            'You may need a newer version of desktop Chrome. ' +
            'Please try Chrome ' + browser.adbBrowserVersion + ' or later.';
          browserHeader.appendChild(warningSection);
        } else if (majorChromeVersion >= MIN_VERSION_NEW_TAB) {
          var newPage = document.createElement('div');
          newPage.className = 'open';

          var newPageUrl = document.createElement('input');
          newPageUrl.type = 'text';
          newPageUrl.placeholder = 'Open tab with url';
          newPage.appendChild(newPageUrl);

          var openHandler = function(sourceId, browserId, input) {
            sendCommand(
                'open', sourceId, browserId, input.value || 'about:blank');
            input.value = '';
          }.bind(null, browser.source, browser.id, newPageUrl);
          newPageUrl.addEventListener('keyup', function(handler, event) {
            if (event.keyIdentifier == 'Enter' && event.target.value)
              handler();
          }.bind(null, openHandler), true);

          var newPageButton = document.createElement('button');
          newPageButton.textContent = 'Open';
          newPage.appendChild(newPageButton);
          newPageButton.addEventListener('click', openHandler, true);

          browserHeader.appendChild(newPage);
        }

        var browserInspector;
        var browserInspectorTitle;
        if ('trace' in queryParamsObject || 'tracing' in queryParamsObject) {
          browserInspector = 'chrome://tracing';
          browserInspectorTitle = 'trace';
        } else {
          browserInspector = queryParamsObject['browser-inspector'];
          browserInspectorTitle = 'inspect';
        }
        if (browserInspector) {
          var link = document.createElement('span');
          link.classList.add('action');
          link.setAttribute('tabindex', 1);
          link.textContent = browserInspectorTitle;
          browserHeader.appendChild(link);
          link.addEventListener(
              'click',
              sendCommand.bind(null, 'inspect-browser', browser.source,
                  browser.id, browserInspector), false);
        }

        pageList = document.createElement('div');
        pageList.className = 'list pages';
        browserSection.appendChild(pageList);
      }

      if (incompatibleVersion || alreadyDisplayed(browserSection, browser))
        continue;

      pageList.textContent = '';
      for (var p = 0; p < browser.pages.length; p++) {
        var page = browser.pages[p];
        // Attached targets have no unique id until Chrome 26. For such targets
        // it is impossible to activate existing DevTools window.
        page.hasNoUniqueId = page.attached &&
            (majorChromeVersion && majorChromeVersion < MIN_VERSION_TARGET_ID);
        var row = addTargetToList(page, pageList, ['name', 'url']);
        if (page['description'])
          addWebViewDetails(row, page);
        else
          addFavicon(row, page);
        if (majorChromeVersion >= MIN_VERSION_TAB_ACTIVATE) {
          addActionLink(row, 'focus tab',
              sendTargetCommand.bind(null, 'activate', page), false);
        }
        if (majorChromeVersion) {
          addActionLink(row, 'reload',
              sendTargetCommand.bind(null, 'reload', page), page.attached);
        }
        if (majorChromeVersion >= MIN_VERSION_TAB_CLOSE) {
          addActionLink(row, 'close',
              sendTargetCommand.bind(null, 'close', page), false);
        }
      }
    }
  }
}

function addToPagesList(data) {
  var row = addTargetToList(data, $('pages-list'), ['name', 'url']);
  addFavicon(row, data);
  if (data.guests)
    addGuestViews(row, data.guests);
}

function addToExtensionsList(data) {
  var row = addTargetToList(data, $('extensions-list'), ['name', 'url']);
  addFavicon(row, data);
  if (data.guests)
    addGuestViews(row, data.guests);
}

function addToAppsList(data) {
  var row = addTargetToList(data, $('apps-list'), ['name', 'url']);
  addFavicon(row, data);
  if (data.guests)
    addGuestViews(row, data.guests);
}

function addGuestViews(row, guests) {
  Array.prototype.forEach.call(guests, function(guest) {
    var guestRow = addTargetToList(guest, row, ['name', 'url']);
    guestRow.classList.add('guest');
    addFavicon(guestRow, guest);
  });
}

function addToWorkersList(data) {
  var row =
      addTargetToList(data, $('workers-list'), ['name', 'description', 'url']);
  addActionLink(row, 'terminate',
      sendTargetCommand.bind(null, 'close', data), false);
}

function addToOthersList(data) {
  addTargetToList(data, $('others-list'), ['url']);
}

function formatValue(data, property) {
  var value = data[property];

  if (property == 'name' && value == '') {
    value = 'untitled';
  }

  var text = value ? String(value) : '';
  if (text.length > 100)
    text = text.substring(0, 100) + '\u2026';

  var div = document.createElement('div');
  div.textContent = text;
  div.className = property;
  return div;
}

function addFavicon(row, data) {
  var favicon = document.createElement('img');
  if (data['faviconUrl'])
    favicon.src = data['faviconUrl'];
  var propertiesBox = row.querySelector('.properties-box');
  propertiesBox.insertBefore(favicon, propertiesBox.firstChild);
}

function addWebViewDetails(row, data) {
  var webview;
  try {
    webview = JSON.parse(data['description']);
  } catch (e) {
    return;
  }
  addWebViewDescription(row, webview);
  if (data.adbScreenWidth && data.adbScreenHeight)
    addWebViewThumbnail(
        row, webview, data.adbScreenWidth, data.adbScreenHeight);
}

function addWebViewDescription(row, webview) {
  var viewStatus = { visibility: '', position: '', size: '' };
  if (!webview.empty) {
    if (webview.attached && !webview.visible)
      viewStatus.visibility = 'hidden';
    else if (!webview.attached)
      viewStatus.visibility = 'detached';
    viewStatus.size = 'size ' + webview.width + ' \u00d7 ' + webview.height;
  } else {
    viewStatus.visibility = 'empty';
  }
  if (webview.attached) {
      viewStatus.position =
        'at (' + webview.screenX + ', ' + webview.screenY + ')';
  }

  var subRow = document.createElement('div');
  subRow.className = 'subrow webview';
  if (webview.empty || !webview.attached || !webview.visible)
    subRow.className += ' invisible-view';
  if (viewStatus.visibility)
    subRow.appendChild(formatValue(viewStatus, 'visibility'));
  if (viewStatus.position)
    subRow.appendChild(formatValue(viewStatus, 'position'));
  subRow.appendChild(formatValue(viewStatus, 'size'));
  var subrowBox = row.querySelector('.subrow-box');
  subrowBox.insertBefore(subRow, row.querySelector('.actions'));
}

function addWebViewThumbnail(row, webview, screenWidth, screenHeight) {
  var maxScreenRectSize = 50;
  var screenRectWidth;
  var screenRectHeight;

  var aspectRatio = screenWidth / screenHeight;
  if (aspectRatio < 1) {
    screenRectWidth = Math.round(maxScreenRectSize * aspectRatio);
    screenRectHeight = maxScreenRectSize;
  } else {
    screenRectWidth = maxScreenRectSize;
    screenRectHeight = Math.round(maxScreenRectSize / aspectRatio);
  }

  var thumbnail = document.createElement('div');
  thumbnail.className = 'webview-thumbnail';
  var thumbnailWidth = 3 * screenRectWidth;
  var thumbnailHeight = 60;
  thumbnail.style.width = thumbnailWidth + 'px';
  thumbnail.style.height = thumbnailHeight + 'px';

  var screenRect = document.createElement('div');
  screenRect.className = 'screen-rect';
  screenRect.style.left = screenRectWidth + 'px';
  screenRect.style.top = (thumbnailHeight - screenRectHeight) / 2 + 'px';
  screenRect.style.width = screenRectWidth + 'px';
  screenRect.style.height = screenRectHeight + 'px';
  thumbnail.appendChild(screenRect);

  if (!webview.empty && webview.attached) {
    var viewRect = document.createElement('div');
    viewRect.className = 'view-rect';
    if (!webview.visible)
      viewRect.classList.add('hidden');
    function percent(ratio) {
      return ratio * 100 + '%';
    }
    viewRect.style.left = percent(webview.screenX / screenWidth);
    viewRect.style.top = percent(webview.screenY / screenHeight);
    viewRect.style.width = percent(webview.width / screenWidth);
    viewRect.style.height = percent(webview.height / screenHeight);
    screenRect.appendChild(viewRect);
  }

  var propertiesBox = row.querySelector('.properties-box');
  propertiesBox.insertBefore(thumbnail, propertiesBox.firstChild);
}

function addTargetToList(data, list, properties) {
  var row = document.createElement('div');
  row.className = 'row';

  var propertiesBox = document.createElement('div');
  propertiesBox.className = 'properties-box';
  row.appendChild(propertiesBox);

  var subrowBox = document.createElement('div');
  subrowBox.className = 'subrow-box';
  propertiesBox.appendChild(subrowBox);

  var subrow = document.createElement('div');
  subrow.className = 'subrow';
  subrowBox.appendChild(subrow);

  for (var j = 0; j < properties.length; j++)
    subrow.appendChild(formatValue(data, properties[j]));

  var actionBox = document.createElement('div');
  actionBox.className = 'actions';
  subrowBox.appendChild(actionBox);

  if (!data.hasCustomInspectAction) {
    addActionLink(row, 'inspect', sendTargetCommand.bind(null, 'inspect', data),
        data.hasNoUniqueId || data.adbAttachedForeign);
  }

  list.appendChild(row);
  return row;
}

function addActionLink(row, text, handler, opt_disabled) {
  var link = document.createElement('span');
  link.classList.add('action');
  link.setAttribute('tabindex', 1);
  if (opt_disabled)
    link.classList.add('disabled');
  else
    link.classList.remove('disabled');

  link.textContent = text;
  link.addEventListener('click', handler, true);
  function handleKey(e) {
    if (e.keyIdentifier == 'Enter' || e.keyIdentifier == 'U+0020') {
      e.preventDefault();
      handler();
    }
  }
  link.addEventListener('keydown', handleKey, true);
  row.querySelector('.actions').appendChild(link);
}


function initSettings() {
  $('discover-usb-devices-enable').addEventListener('change',
                                                    enableDiscoverUsbDevices);

  $('port-forwarding-enable').addEventListener('change', enablePortForwarding);
  $('port-forwarding-config-open').addEventListener(
      'click', openPortForwardingConfig);
  $('port-forwarding-config-close').addEventListener(
      'click', closePortForwardingConfig);
  $('port-forwarding-config-done').addEventListener(
      'click', commitPortForwardingConfig.bind(true));
}

function enableDiscoverUsbDevices(event) {
  sendCommand('set-discover-usb-devices-enabled', event.target.checked);
}

function enablePortForwarding(event) {
  sendCommand('set-port-forwarding-enabled', event.target.checked);
}

function handleKey(event) {
  switch (event.keyCode) {
    case 13:  // Enter
      if (event.target.nodeName == 'INPUT') {
        var line = event.target.parentNode;
        if (!line.classList.contains('fresh') ||
            line.classList.contains('empty')) {
          commitPortForwardingConfig(true);
        } else {
          commitFreshLineIfValid(true /* select new line */);
          commitPortForwardingConfig(false);
        }
      } else {
        commitPortForwardingConfig(true);
      }
      break;

    case 27:
      commitPortForwardingConfig(true);
      break;
  }
}

function setModal(dialog) {
  dialog.deactivatedNodes = Array.prototype.filter.call(
      document.querySelectorAll('*'),
      function(n) {
        return n != dialog && !dialog.contains(n) && n.tabIndex >= 0;
      });

  dialog.tabIndexes = dialog.deactivatedNodes.map(
    function(n) { return n.getAttribute('tabindex'); });

  dialog.deactivatedNodes.forEach(function(n) { n.tabIndex = -1; });
  window.modal = dialog;
}

function unsetModal(dialog) {
  for (var i = 0; i < dialog.deactivatedNodes.length; i++) {
    var node = dialog.deactivatedNodes[i];
    if (dialog.tabIndexes[i] === null)
      node.removeAttribute('tabindex');
    else
      node.setAttribute('tabindex', dialog.tabIndexes[i]);
  }

  if (window.holdDevices) {
    populateRemoteTargets(window.holdDevices);
    delete window.holdDevices;
  }

  delete dialog.deactivatedNodes;
  delete dialog.tabIndexes;
  delete window.modal;
}

function openPortForwardingConfig() {
  loadPortForwardingConfig(window.portForwardingConfig);

  $('port-forwarding-overlay').classList.add('open');
  document.addEventListener('keyup', handleKey);

  var freshPort = document.querySelector('.fresh .port');
  if (freshPort)
    freshPort.focus();
  else
    $('port-forwarding-config-done').focus();

  setModal($('port-forwarding-overlay'));
}

function closePortForwardingConfig() {
  if (!$('port-forwarding-overlay').classList.contains('open'))
    return;

  $('port-forwarding-overlay').classList.remove('open');
  document.removeEventListener('keyup', handleKey);
  unsetModal($('port-forwarding-overlay'));
}

function loadPortForwardingConfig(config) {
  var list = $('port-forwarding-config-list');
  list.textContent = '';
  for (var port in config)
    list.appendChild(createConfigLine(port, config[port]));
  list.appendChild(createEmptyConfigLine());
}

function commitPortForwardingConfig(closeConfig) {
  if (closeConfig)
    closePortForwardingConfig();

  commitFreshLineIfValid();
  var lines = document.querySelectorAll('.port-forwarding-pair');
  var config = {};
  for (var i = 0; i != lines.length; i++) {
    var line = lines[i];
    var portInput = line.querySelector('.port');
    var locationInput = line.querySelector('.location');

    var port = portInput.classList.contains('invalid') ?
               portInput.lastValidValue :
               portInput.value;

    var location = locationInput.classList.contains('invalid') ?
                   locationInput.lastValidValue :
                   locationInput.value;

    if (port && location)
      config[port] = location;
  }
  sendCommand('set-port-forwarding-config', config);
}

function updateDiscoverUsbDevicesEnabled(enabled) {
  var checkbox = $('discover-usb-devices-enable');
  checkbox.checked = !!enabled;
  checkbox.disabled = false;
}

function updatePortForwardingEnabled(enabled) {
  var checkbox = $('port-forwarding-enable');
  checkbox.checked = !!enabled;
  checkbox.disabled = false;
}

function updatePortForwardingConfig(config) {
  window.portForwardingConfig = config;
  $('port-forwarding-config-open').disabled = !config;
}

function createConfigLine(port, location) {
  var line = document.createElement('div');
  line.className = 'port-forwarding-pair';

  var portInput = createConfigField(port, 'port', 'Port', validatePort);
  line.appendChild(portInput);

  var locationInput = createConfigField(
      location, 'location', 'IP address and port', validateLocation);
  line.appendChild(locationInput);
  locationInput.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0009' &&  // Tab
        !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey &&
        line.classList.contains('fresh') &&
        !line.classList.contains('empty')) {
      // Tabbing forward on the fresh line, try create a new empty one.
      if (commitFreshLineIfValid(true))
        e.preventDefault();
    }
  });

  var lineDelete = document.createElement('div');
  lineDelete.className = 'close-button';
  lineDelete.addEventListener('click', function() {
    var newSelection = line.nextElementSibling;
    line.parentNode.removeChild(line);
    selectLine(newSelection);
  });
  line.appendChild(lineDelete);

  line.addEventListener('click', selectLine.bind(null, line));
  line.addEventListener('focus', selectLine.bind(null, line));

  checkEmptyLine(line);

  return line;
}

function validatePort(input) {
  var match = input.value.match(/^(\d+)$/);
  if (!match)
    return false;
  var port = parseInt(match[1]);
  if (port < 1024 || 65535 < port)
    return false;

  var inputs = document.querySelectorAll('input.port:not(.invalid)');
  for (var i = 0; i != inputs.length; ++i) {
    if (inputs[i] == input)
      break;
    if (parseInt(inputs[i].value) == port)
      return false;
  }
  return true;
}

function validateLocation(input) {
  var match = input.value.match(/^([a-zA-Z0-9\.\-_]+):(\d+)$/);
  if (!match)
    return false;
  var port = parseInt(match[2]);
  return port <= 65535;
}

function createEmptyConfigLine() {
  var line = createConfigLine('', '');
  line.classList.add('fresh');
  return line;
}

function createConfigField(value, className, hint, validate) {
  var input = document.createElement('input');
  input.className = className;
  input.type = 'text';
  input.placeholder = hint;
  input.value = value;
  input.lastValidValue = value;

  function checkInput() {
    if (validate(input))
      input.classList.remove('invalid');
    else
      input.classList.add('invalid');
    if (input.parentNode)
      checkEmptyLine(input.parentNode);
  }
  checkInput();

  input.addEventListener('keyup', checkInput);
  input.addEventListener('focus', function() {
    selectLine(input.parentNode);
  });

  input.addEventListener('blur', function() {
    if (validate(input))
      input.lastValidValue = input.value;
  });

  return input;
}

function checkEmptyLine(line) {
  var inputs = line.querySelectorAll('input');
  var empty = true;
  for (var i = 0; i != inputs.length; i++) {
    if (inputs[i].value != '')
      empty = false;
  }
  if (empty)
    line.classList.add('empty');
  else
    line.classList.remove('empty');
}

function selectLine(line) {
  if (line.classList.contains('selected'))
    return;
  unselectLine();
  line.classList.add('selected');
}

function unselectLine() {
  var line = document.querySelector('.port-forwarding-pair.selected');
  if (!line)
    return;
  line.classList.remove('selected');
  commitFreshLineIfValid();
}

function commitFreshLineIfValid(opt_selectNew) {
  var line = document.querySelector('.port-forwarding-pair.fresh');
  if (line.querySelector('.invalid'))
    return false;
  line.classList.remove('fresh');
  var freshLine = createEmptyConfigLine();
  line.parentNode.appendChild(freshLine);
  if (opt_selectNew)
    freshLine.querySelector('.port').focus();
  return true;
}

function populatePortStatus(devicesStatusMap) {
  for (var deviceId in devicesStatusMap) {
    if (!devicesStatusMap.hasOwnProperty(deviceId))
      continue;
    var deviceStatusMap = devicesStatusMap[deviceId];

    var deviceSection = $(deviceId);
    if (!deviceSection)
      continue;

    var devicePorts = deviceSection.querySelector('.device-ports');
    devicePorts.textContent = '';
    for (var port in deviceStatusMap) {
      if (!deviceStatusMap.hasOwnProperty(port))
        continue;

      var status = deviceStatusMap[port];
      var portIcon = document.createElement('div');
      portIcon.className = 'port-icon';
      // status === 0 is the default (connected) state.
      // Positive values correspond to the tunnelling connection count
      // (in DEBUG_DEVTOOLS mode).
      if (status > 0)
        portIcon.classList.add('connected');
      else if (status === -1 || status === -2)
        portIcon.classList.add('transient');
      else if (status < 0)
        portIcon.classList.add('error');
      devicePorts.appendChild(portIcon);

      var portNumber = document.createElement('div');
      portNumber.className = 'port-number';
      portNumber.textContent = ':' + port;
      if (status > 0)
        portNumber.textContent += '(' + status + ')';
      devicePorts.appendChild(portNumber);
    }
  }

  function clearPorts(deviceSection) {
    if (deviceSection.id in devicesStatusMap)
      return;
    deviceSection.querySelector('.device-ports').textContent = '';
  }

  Array.prototype.forEach.call(
      document.querySelectorAll('.device'), clearPorts);
}

document.addEventListener('DOMContentLoaded', onload);

window.addEventListener('hashchange', onHashChange);
