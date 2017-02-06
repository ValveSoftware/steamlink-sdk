# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utilities for capturing traces for chromecast devices."""

import json
import logging
import math
import websocket


class TracingClient(object):

  def BufferUsage(self, buffer_usage):
    percent = int(math.floor(buffer_usage * 100))
    logging.debug('Buffer Usage: %i', percent)


class TracingBackend(object):
  """Class for starting a tracing session with cast_shell."""

  def __init__(self):
    self._socket = None
    self._next_request_id = 0
    self._tracing_client = None
    self._tracing_data = []

  def Connect(self, device_ip, devtools_port=9222, timeout=10):
    """Connect to cast_shell on given device and port.

    Args:
      device_ip: IP of device to connect to.
      devtools_port: Remote dev tool port to connect to. Defaults to 9222.
      timeout: Amount of time to wait for connection in seconds. Default 10s.
    """
    assert not self._socket
    url = 'ws://%s:%i/devtools/browser' % (device_ip, devtools_port)
    print('Connect to %s ...' % url)
    self._socket = websocket.create_connection(url, timeout=timeout)
    self._next_request_id = 0

  def Disconnect(self):
    """If connected to device, disconnect from device."""
    if self._socket:
      self._socket.close()
      self._socket = None

  def StartTracing(self,
                   tracing_client=None,
                   custom_categories=None,
                   record_continuously=False,
                   buffer_usage_reporting_interval=0,
                   timeout=10):
    """Begin a tracing session on device.

    Args:
      tracing_client: client for this tracing session.
      custom_categories: Categories to filter for. None records all categories.
      record_continuously: Keep tracing until stopped. If false, will exit when
                           buffer is full.
      buffer_usage_reporting_interval: How often to report buffer usage.
      timeout: Time to wait to start tracing in seconds. Default 10s.
    """
    self._tracing_client = tracing_client
    self._socket.settimeout(timeout)
    req = {
      'method': 'Tracing.start',
      'params': {
        'categories': custom_categories,
        'bufferUsageReportingInterval': buffer_usage_reporting_interval,
        'options': 'record-continuously' if record_continuously else
                   'record-until-full'
      }
    }
    self._SendRequest(req)

  def StopTracing(self, timeout=30):
    """End a tracing session on device.

    Args:
      timeout: Time to wait to stop tracing in seconds. Default 30s.

    Returns:
      Trace file for the stopped session.
    """
    self._socket.settimeout(timeout)
    req = {'method': 'Tracing.end'}
    self._SendRequest(req)
    while self._socket:
      res = self._ReceiveResponse()
      if 'method' in res and self._HandleResponse(res):
        self._tracing_client = None
        result = self._tracing_data
        self._tracing_data = []
        return result

  def _SendRequest(self, req):
    """Sends request to remote devtools.

    Args:
      req: Request to send.
    """
    req['id'] = self._next_request_id
    self._next_request_id += 1
    data = json.dumps(req)
    self._socket.send(data)

  def _ReceiveResponse(self):
    """Get response from remote devtools.

    Returns:
      Response received.
    """
    while self._socket:
      data = self._socket.recv()
      res = json.loads(data)
      return res

  def _HandleResponse(self, res):
    """Handle response from remote devtools.

    Args:
      res: Recieved tresponse that should be handled.
    """
    method = res.get('method')
    value = res.get('params', {}).get('value')
    if 'Tracing.dataCollected' == method:
      if type(value) in [str, unicode]:
        self._tracing_data.append(value)
      elif type(value) is list:
        self._tracing_data.extend(value)
      else:
        logging.warning('Unexpected type in tracing data')
    elif 'Tracing.bufferUsage' == method and self._tracing_client:
      self._tracing_client.BufferUsage(value)
    elif 'Tracing.tracingComplete' == method:
      return True
