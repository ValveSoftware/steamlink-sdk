// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    function(message, sender, sendResponse) {
      function doSendResponse(value, error) {
        var errorMessage = error || chrome.extension.lastError;
        sendResponse({'value': value, 'error': errorMessage});
      }

      function getHost(url) {
        if (!url)
          return '';
        // Use the DOM to parse the URL. Since we don't add the anchor to
        // the page, this is the only reference to it and it will be
        // deleted once it's gone out of scope.
        var a = document.createElement('a');
        a.href = url;
        var origin = a.protocol + '//' + a.hostname;
        if (a.port != '')
          origin = origin + ':' + a.port;
        origin = origin + '/';
        return origin;
      }

      try {
        var method = message['method'];
        var origin = getHost(sender.url);
        if (method == 'chooseDesktopMedia') {
          // TODO(bemasc): Remove this method once the caller has transitioned
          // to using the port.
          var cancelId;
          function sendResponseWithCancelId(streamId) {
            var value = {'cancelId': cancelId, 'streamId': streamId};
            doSendResponse(value);
          }
          cancelId = chrome.desktopCapture.chooseDesktopMedia(
              ['screen', 'window'], sender.tab, sendResponseWithCancelId);
          return true;
        } else if (method == 'cancelChooseDesktopMedia') {
          // TODO(bemasc): Remove this method (see above).
          var cancelId = message['cancelId'];
          chrome.desktopCapture.cancelChooseDesktopMedia(cancelId);
          doSendResponse();
          return false;
        } else if (method == 'cpu.getInfo') {
          chrome.system.cpu.getInfo(doSendResponse);
          return true;
        } else if (method == 'logging.setMetadata') {
          var metaData = message['metaData'];
          chrome.webrtcLoggingPrivate.setMetaData(
              sender.tab.id, origin, metaData, doSendResponse);
          return true;
        } else if (method == 'logging.start') {
          chrome.webrtcLoggingPrivate.start(
              sender.tab.id, origin, doSendResponse);
          return true;
        } else if (method == 'logging.uploadOnRenderClose') {
          chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
              sender.tab.id, origin, true);
          doSendResponse();
          return false;
        } else if (method == 'logging.noUploadOnRenderClose') {
          chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
              sender.tab.id, origin, false);
          doSendResponse();
          return false;
        } else if (method == 'logging.stop') {
          chrome.webrtcLoggingPrivate.stop(
              sender.tab.id, origin, doSendResponse);
          return true;
        } else if (method == 'logging.upload') {
          chrome.webrtcLoggingPrivate.upload(
              sender.tab.id, origin, doSendResponse);
          return true;
        } else if (method == 'logging.stopAndUpload') {
          stopAllRtpDump(sender.tab.id, origin, function() {
            chrome.webrtcLoggingPrivate.stop(sender.tab.id, origin, function() {
              chrome.webrtcLoggingPrivate.upload(
                  sender.tab.id, origin, doSendResponse);
            });
          });
          return true;
        } else if (method == 'logging.discard') {
          chrome.webrtcLoggingPrivate.discard(
              sender.tab.id, origin, doSendResponse);
          return true;
        } else if (method == 'getSinks') {
          chrome.webrtcAudioPrivate.getSinks(doSendResponse);
          return true;
        } else if (method == 'getActiveSink') {
          chrome.webrtcAudioPrivate.getActiveSink(
              sender.tab.id, doSendResponse);
          return true;
        } else if (method == 'setActiveSink') {
          var sinkId = message['sinkId'];
          chrome.webrtcAudioPrivate.setActiveSink(
              sender.tab.id, sinkId, doSendResponse);
          return true;
        } else if (method == 'getAssociatedSink') {
          var sourceId = message['sourceId'];
          chrome.webrtcAudioPrivate.getAssociatedSink(
              origin, sourceId, doSendResponse);
          return true;
        } else if (method == 'isExtensionEnabled') {
          // This method is necessary because there may be more than one
          // version of this extension, under different extension IDs. By
          // first calling this method on the extension ID, the client can
          // check if it's loaded; if it's not, the extension system will
          // call the callback with no arguments and set
          // chrome.runtime.lastError.
          doSendResponse();
          return false;
        } else if (method == 'getNaclArchitecture') {
          chrome.runtime.getPlatformInfo(function(obj) {
            doSendResponse(obj.nacl_arch);
          });
          return true;
        } else if (method == 'logging.startRtpDump') {
          var incoming = message['incoming'] || false;
          var outgoing = message['outgoing'] || false;
          chrome.webrtcLoggingPrivate.startRtpDump(
              sender.tab.id, origin, incoming, outgoing, doSendResponse);
          return true;
        } else if (method == 'logging.stopRtpDump') {
          var incoming = message['incoming'] || false;
          var outgoing = message['outgoing'] || false;
          chrome.webrtcLoggingPrivate.stopRtpDump(
              sender.tab.id, origin, incoming, outgoing, doSendResponse);
          return true;
        }
        throw new Error('Unknown method: ' + method);
      } catch (e) {
        doSendResponse(null, e.name + ': ' + e.message);
      }
    }
);

// If Hangouts connects with a port named 'onSinksChangedListener', we
// will register a listener and send it a message {'eventName':
// 'onSinksChanged'} whenever the event fires.
function onSinksChangedPort(port) {
  function clientListener() {
    port.postMessage({'eventName': 'onSinksChanged'});
  }
  chrome.webrtcAudioPrivate.onSinksChanged.addListener(clientListener);

  port.onDisconnect.addListener(function() {
    chrome.webrtcAudioPrivate.onSinksChanged.removeListener(
        clientListener);
  });
}

// This is a one-time-use port for calling chooseDesktopMedia.  The page
// sends one message, identifying the requested source types, and the
// extension sends a single reply, with the user's selected streamId.  A port
// is used so that if the page is closed before that message is sent, the
// window picker dialog will be closed.
function onChooseDesktopMediaPort(port) {
  function sendResponse(streamId) {
    port.postMessage({'value': {'streamId': streamId}});
    port.disconnect();
  }

  port.onMessage.addListener(function(message) {
    var method = message['method'];
    if (method == 'chooseDesktopMedia') {
      var sources = message['sources'];
      var cancelId = chrome.desktopCapture.chooseDesktopMedia(
          sources, port.sender.tab, sendResponse);
      port.onDisconnect.addListener(function() {
        // This method has no effect if called after the user has selected a
        // desktop media source, so it does not need to be conditional.
        chrome.desktopCapture.cancelChooseDesktopMedia(cancelId);
      });
    }
  });
}

// A port for continuously reporting relevant CPU usage information to the page.
function onProcessCpu(port) {
  var tabPid;
  function processListener(processes) {
    if (tabPid == undefined) {
      // getProcessIdForTab sometimes fails, and does not call the callback.
      // (Tracked at https://crbug.com/368855.)
      // This call retries it on each process update until it succeeds.
      chrome.processes.getProcessIdForTab(port.sender.tab.id, function(x) {
        tabPid = x;
      });
      return;
    }
    var tabProcess = processes[tabPid];
    if (!tabProcess) {
      return;
    }
    var pluginProcessCpu = 0, browserProcessCpu = 0, gpuProcessCpu = 0;
    for (var pid in processes) {
      var process = processes[pid];
      if (process.type == 'browser') {
        browserProcessCpu = process.cpu;
      } else if (process.type == 'gpu') {
        gpuProcessCpu = process.cpu;
      } else if ((process.type == 'plugin' || process.type == 'nacl') &&
                 process.title.toLowerCase().indexOf('hangouts') > 0) {
        pluginProcessCpu = process.cpu;
      }
    }

    port.postMessage({
      'tabCpuUsage': tabProcess.cpu,
      'browserCpuUsage': browserProcessCpu,
      'gpuCpuUsage': gpuProcessCpu,
      'pluginCpuUsage': pluginProcessCpu
    });
  }

  chrome.processes.onUpdated.addListener(processListener);
  port.onDisconnect.addListener(function() {
    chrome.processes.onUpdated.removeListener(processListener);
  });
}

function stopAllRtpDump(tabId, origin, callback) {
  // Stops incoming and outgoing separately, otherwise stopRtpDump will fail if
  // either type of dump has not been started.
  chrome.webrtcLoggingPrivate.stopRtpDump(
      tabId, origin, true, false,
      function() {
        chrome.webrtcLoggingPrivate.stopRtpDump(
            tabId, origin, false, true, callback);
      });
}

chrome.runtime.onConnectExternal.addListener(function(port) {
  if (port.name == 'onSinksChangedListener') {
    onSinksChangedPort(port);
  } else if (port.name == 'chooseDesktopMedia') {
    onChooseDesktopMediaPort(port);
  } else if (port.name == 'processCpu') {
    onProcessCpu(port);
  } else {
    // Unknown port type.
    port.disconnect();
  }
});
