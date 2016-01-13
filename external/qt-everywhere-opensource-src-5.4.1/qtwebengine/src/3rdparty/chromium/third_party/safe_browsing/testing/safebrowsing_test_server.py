#!/usr/bin/python
#
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Test server for Safebrowsing protocol v2.

To test an implementation of the safebrowsing protocol, this server should
be run on the same machine as the client implementation.  The client should
connect to this server at localhost:port where port is specified as a command
line flag (--port) and perform updates normally, except that each request
should have an additional CGI param "test_step" that specifies which update
request this is for the client.  That is, it should be incremented after the
complete parsing of a downloads request so a downloads request and its
associated redirects should all have the same test_step. The client should
also make a newkey request and then a getlists requests before making the
first update request and should use test_step=1 for these requests (test_step
is 1 indexed). When the client believes that it is done with testing (because
it recieves a response from an update request with no new data), it should
make a "/test_complete" request. This will return either "yes" or "no" if the
test is complete or not.
"""

__author__ = 'gcasto@google.com (Garrett Casto)'

import BaseHTTPServer
import binascii
import base64
import cgi
import hmac
from optparse import OptionParser
import re
import sha
import sys
from threading import Timer
import time
import urlparse

import external_test_pb2

DEFAULT_PORT = 40101
DEFAULT_DATAFILE_LOCATION = "testing_input.dat"
POST_DATA_KEY = "post_data"
GETHASH_PATH = "/safebrowsing/gethash"
RESET_PATH="/reset"
DOWNLOADS_PATH = "/safebrowsing/downloads"
TEST_COMPLETE_PATH = "/test_complete"
DATABASE_VALIDATION_PATH = "/safebrowsing/verify_database"

# Dict of step -> List of (request_path, param key, response)
response_data_by_step = {}
# Dict of step -> Dict of hash_prefix ->
# (full length hashes responses, num times requested)
hash_data_by_step = {}
client_key = None
enforce_caching = False
validate_database = True
datafile_location = ''

def EndServer():
  sys.exit(0)

def CGIParamsToListOfTuples(cgi_params):
  return [(param.Name, param.Value) for param in cgi_params]

def SortedTupleFromParamsAndPostData(params,
                                     post_data):
  """ Make a sorted tuple from the request such that it can be inserted as
  a key in a map. params is a list of (name, value) tuples and post_data is
  a string (or None).
  """
  if post_data:
    params.append((POST_DATA_KEY, tuple(sorted(post_data.split('\n')))))
  return tuple(sorted(params))

def LoadData():
  """ Load data from datafile_location to be used by the testing server.
  """
  global response_data_by_step
  global hash_data_by_step
  global client_key
  data_file = open(datafile_location, 'rb')
  str_data = data_file.read()
  test_data = external_test_pb2.TestData()
  test_data.ParseFromString(str_data)
  print "Data Loaded"
  if test_data.HasField('ClientKey'):
    client_key = test_data.ClientKey
  else:
    client_key = None
  step = 0
  response_data_by_step = {}
  hash_data_by_step = {}
  for step_data in test_data.Steps:
    step += 1
    step_list = []
    for request_data in step_data.Requests:
      params_tuple = SortedTupleFromParamsAndPostData(
          CGIParamsToListOfTuples(request_data.Params),
          request_data.PostData)
      step_list.append((request_data.RequestPath,
                        params_tuple,
                        request_data.ServerResponse))
    response_data_by_step[step] = step_list

    hash_step_dict = {}
    for hash_request in step_data.Hashes:
      hash_step_dict[hash_request.HashPrefix] = (hash_request.ServerResponse,
                                                 hash_request.Expression,
                                                 0)
    hash_data_by_step[step] = hash_step_dict
  print "Data Parsed"

def VerifyTestComplete():
  """ Returns true if all the necessary requests have been made by the client.
  """
  global response_data_by_step
  global hash_data_by_step
  global enforce_caching

  complete = True
  for (step, step_list) in response_data_by_step.iteritems():
    if len(step_list):
      print ("Step %s has %d request(s) that were not made %s" %
             (step, len(step_list), step_list))
      complete = False

  for (step, hash_step_dict) in hash_data_by_step.iteritems():
    for (prefix,
         (response, expression, num_requests)) in hash_step_dict.iteritems():
      if ((enforce_caching and num_requests != 1) or
          num_requests == 0):
        print ("Hash prefix %s not requested the correct number of times"
               "(%d requests). Requests originated because of expression"
               " %s. Prefix is located in the following locations" %
               (binascii.hexlify(prefix),
                num_requests,
                expression))
        # This information is slightly redundant with what will be printed below
        # but it is occasionally worth seeing.
        print "Response %s" % response
        cur_index = 0
        while cur_index < len(response):
          end_header_index = response.find('\n', cur_index + 1)
          header = response[cur_index:end_header_index]
          (listname, chunk_num, hashdatalen) = header.split(":")
          print "   List '%s' in add chunk num %s" % (listname, chunk_num)
          cur_index = end_header_index + int(hashdatalen) + 1

        complete = False

  # TODO(gcasto): Have a check here that verifies that the client doesn't
  # make too many hash requests during the test run.

  return complete

class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  def ParamDictToListOfTuples(self, params):
    # params is a list cgi params to list of specified values.  Since we never
    # expect a parameter to be specified multiple times, we just take the first
    # one.
    return [(name, value[0]) for (name, value) in params.iteritems()]

  def MakeParamKey(self, params, post_data=None):
    """ Make a lookup key from the request.
    """
    return SortedTupleFromParamsAndPostData(
        self.ParamDictToListOfTuples(params),
        post_data)

  def MACResponse(self, response, is_downloads_request):
    """ Returns the response wrapped with a MAC. Formatting will change
    if this is a downloads_request or hashserver_request.  If no client_key
    is set, returns the response as-is.
    """
    if client_key is None:
      return response
    unescaped_mac = hmac.new(client_key, response, sha).digest()
    return "%s%s\n%s" % (is_downloads_request and "m:" or "",
                       base64.urlsafe_b64encode(unescaped_mac),
                       response)

  def VerifyRequest(self, is_post_request):
    """ Verify that the request matches one loaded from the datafile and
    give the corresponding response. If there is no match, try and give a
    descriptive error message in the response.
    """
    parsed_url = urlparse.urlparse(self.path)
    path = parsed_url[2]
    params = cgi.parse_qs(parsed_url[4])

    step = params.get("test_step")
    if step is None or len(step) != 1:
      self.send_response(400)
      self.end_headers()
      print "No test step present."
      return
    step = int(step[0])

    if path == TEST_COMPLETE_PATH:
      self.send_response(200)
      self.end_headers()
      if VerifyTestComplete():
        self.wfile.write('yes')
      else:
        self.wfile.write('no')
    elif path == GETHASH_PATH:
      self.SynthesizeGethashResponse(step)
    elif path == RESET_PATH:
      LoadData()
      self.send_response(200)
      self.end_headers()
      self.wfile.write('done')
    else:
      self.GetCannedResponse(path, params, step, is_post_request)

  def SynthesizeGethashResponse(self, step):
    """ Create a gethash response. This will possibly combine an arbitrary
    number of hash requests from the protocol buffer.
    """
    global hash_data_by_step

    hashes_for_step = hash_data_by_step.get(step, {})
    if not hashes_for_step:
      self.send_response(400)
      self.end_headers()
      print "No response for step %d" % step
      return

    post_data = self.rfile.read(int(self.headers['Content-Length']))
    match = re.match(
        r'(?P<prefixsize>\d+):(?P<totalsize>\d+)\n(?P<prefixes>.+)',
        post_data,
        re.MULTILINE | re.IGNORECASE | re.DOTALL)
    if not match:
      self.send_response(400)
      self.end_headers()
      print "Gethash request is malformed %s" % post_data
      return

    prefixsize = int(match.group('prefixsize'))
    total_length = int(match.group('totalsize'))
    if total_length % prefixsize != 0:
      self.send_response(400)
      self.end_headers()
      print ("Gethash request is malformed, length should be a multiple of the "
             " prefix size%s" % post_data)
      return

    response = ""
    for n in range(total_length/prefixsize):
      prefix = match.group('prefixes')[n*prefixsize:n*prefixsize + prefixsize]
      hash_data = hashes_for_step.get(prefix)
      if hash_data is not None:
        # Reply with the correct response
        response += hash_data[0]
        # Remember that this hash has now been requested.
        hashes_for_step[prefix] = (hash_data[0], hash_data[1], hash_data[2] + 1)

    if not response:
      self.send_response(204)
      self.end_headers()
      return

    # Need to perform MACing before sending response out.
    self.send_response(200)
    self.end_headers()
    self.wfile.write(self.MACResponse(response, False))

  def GetCannedResponse(self, path, params, step, is_post_request):
    """ Given the parameters of a request, see if a matching response is
    found. If one is found, respond with with it, else respond with a 400.
    """
    responses_for_step = response_data_by_step.get(step)
    if not responses_for_step:
      self.send_response(400)
      self.end_headers()
      print "No responses for step %d" % step
      return

    # Delete unnecessary params
    del params["test_step"]
    if "client" in params:
      del params["client"]
    if "appver" in params:
      del params["appver"]
    if "key" in params:
      del params["key"]

    param_key = self.MakeParamKey(
        params,
        is_post_request and
        self.rfile.read(int(self.headers['Content-Length'])) or
        None)

    (expected_path, expected_params, server_response) = responses_for_step[0]
    if expected_path != path or param_key != expected_params:
      self.send_response(400)
      self.end_headers()
      print "Expected request with path %s and params %s." % (expected_path,
                                                              expected_params)
      print "Actual request path %s and params %s" % (path, param_key)
      return

    # Remove request that was just made
    responses_for_step.pop(0)

    # If the next request is not needed for this test run, remove it now.
    # We do this after processing instead of before for cases where the
    # data we are removing is the last requests in a step.
    if responses_for_step:
      (expected_path, _, _) = responses_for_step[0]
      if expected_path == DATABASE_VALIDATION_PATH and not validate_database:
        responses_for_step.pop(0)

    if path == DOWNLOADS_PATH:
      # Need to have the redirects point to the current port.
      server_response = re.sub(r'localhost:\d+',
                               '%s:%d' % (self.server.server_address[0],
                                          self.server.server_port),
                               server_response)
      # Remove the current MAC, because it's going to be wrong now.
      if server_response.startswith('m:'):
        server_response = server_response[server_response.find('\n')+1:]
      # Add a new correct MAC.
      server_response = self.MACResponse(server_response, True)

    self.send_response(200)
    self.end_headers()
    self.wfile.write(server_response)

  def do_GET(self):
    self.VerifyRequest(False)

  def do_POST(self):
    self.VerifyRequest(True)


def SetupServer(opt_datafile_location,
                host,
                port,
                opt_enforce_caching,
                opt_validate_database):
  """Sets up the safebrowsing test server.

  Arguments:
    datafile_location: The file to load testing data from.
    port: port that the server runs on.
    opt_enforce_caching: Whether to require the client to implement caching.
    opt_validate_database: Whether to require the client makes database
         verification requests.

  Returns:
    An HTTPServer object which the caller should call serve_forever() on.
  """
  global datafile_location
  datafile_location = opt_datafile_location
  LoadData()
  # TODO(gcasto):  Look into extending HTTPServer to remove global variables.
  global enforce_caching
  global validate_database
  enforce_caching = opt_enforce_caching
  validate_database = opt_validate_database
  return BaseHTTPServer.HTTPServer((host, port), RequestHandler)

if __name__ == '__main__':
  parser = OptionParser()
  parser.add_option("--datafile", dest="datafile_location",
                    default=DEFAULT_DATAFILE_LOCATION,
                    help="Location to load testing data from.")
  parser.add_option("--host", dest="host",
                    default='localhost', help="Host the server should bind.")
  parser.add_option("--port", dest="port", type="int",
                    default=DEFAULT_PORT, help="Port to run the server on.")
  parser.add_option("--enforce_caching", dest="enforce_caching",
                    action="store_true", default=False,
                    help="Whether to require that the client"
                    "has implemented caching or not.")
  parser.add_option("--ignore_database_validation", dest="validate_database",
                    action="store_false", default=True,
                    help="Whether to requires that the client makes verify "
                    "database requests or not.")
  parser.add_option("--server_timeout_sec", dest="server_timeout_sec",
                    type="int", default=600,
                    help="How long to let the server run before shutting it "
                    "down. If <=0, the server will never be down")
  (options, _) = parser.parse_args()

  server = SetupServer(options.datafile_location,
                       options.host,
                       options.port,
                       options.enforce_caching,
                       options.validate_database)

  if (options.server_timeout_sec > 0):
    tm = Timer(options.server_timeout_sec, EndServer)
    tm.start()

  try:
    server.serve_forever()
  except KeyboardInterrupt:
    pass
  server.server_close()
  print "Server stopped."
