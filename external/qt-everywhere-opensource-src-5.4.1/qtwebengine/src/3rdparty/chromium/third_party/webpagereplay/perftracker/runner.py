#!/usr/bin/env python
# Copyright 2010 Google Inc. All Rights Reserved.
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

description = """
    This is a script for running automated network tests of chrome.

    There is an optional -e <filename> flag that instead runs an automated
    web-page-replay test. It runs WPR record mode on the set of URLs specified
    in the config file, then runs replay mode on the same set of URLs and
    records any cache misses to <filename>.
"""

import sys
if sys.version < '2.6':
  print 'Need Python 2.6 or greater.'
  sys.exit(1)

import cookielib
import getpass
import json
import logging
import optparse
import os
import platform
import shutil
import signal
import subprocess
import tempfile
import time
import urllib
import urllib2
import runner_cfg


# Some constants for our program

# The location of the Replay script
REPLAY_PATH = '../replay.py'

# The name of the application we're using
BENCHMARK_APPLICATION_NAME = 'perftracker'

# The location of the PerfTracker extension
PERFTRACKER_EXTENSION_PATH = './extension'

# The server_port is the port which runs the webserver to test against.
SERVER_PORT = 7171
SERVER_PORT_SSL = 7172

# For SPDY testing, where we have both a frontend and backend server,
# this is the port to run the backend server.
BACKEND_SERVER_PORT = 7173


def DoAppEngineLogin(username, password):
  """Log into the Google AppEngine app.

  This code credit to: http://dalelane.co.uk/blog/?p=303
  """
  target_authenticated_url = runner_cfg.appengine_url

  # We use a cookie to authenticate with Google App Engine by registering a
  # cookie handler here, this will automatically store the cookie returned
  # when we use urllib2 to open http://<server>.appspot.com/_ah/login
  cookiejar = cookielib.LWPCookieJar()
  opener = urllib2.build_opener(urllib2.HTTPCookieProcessor(cookiejar))
  urllib2.install_opener(opener)

  #
  # get an AuthToken from Google accounts
  #
  try:
    auth_uri = 'https://www.google.com/accounts/ClientLogin'
    authreq_data = urllib.urlencode({ 'Email':   username,
                                      'Passwd':  password,
                                      'service': 'ah',
                                      'source':  BENCHMARK_APPLICATION_NAME,
                                      'accountType': 'HOSTED_OR_GOOGLE' })
    auth_req = urllib2.Request(auth_uri, data=authreq_data)
    auth_resp = urllib2.urlopen(auth_req)
    auth_resp_body = auth_resp.read()
    # The auth response includes several fields.  The part  we're
    # interested in is the bit after 'Auth='.
    auth_resp_dict = dict(x.split('=') for x in auth_resp_body.split('\n') if x)
    authtoken = auth_resp_dict['Auth']

    # Get a cookie:
    # The call to request a cookie will also automatically redirect us to
    # the page that we want to go to. The cookie jar will automatically
    # provide the cookie when we reach the redirected location.
    serv_args = {}
    serv_args['continue'] = target_authenticated_url
    serv_args['auth']     = authtoken
    full_serv_uri = '%s_ah/login?%s' % (target_authenticated_url,
                                        urllib.urlencode(serv_args))

    serv_req = urllib2.Request(full_serv_uri)
    serv_resp = urllib2.urlopen(serv_req)
    print 'AppEngineLogin succeeded.'
  except Exception, e:
    logging.critical('DoAppEngineLogin failed: %s', e)
    return None
  return full_serv_uri


def ClobberTmpDirectory(tmpdir):
  """Remove a temporary directory."""
  # Do sanity checking so we don't clobber the wrong thing
  if tmpdir == '/tmp/' or not tmpdir.startswith('/tmp/'):
    logging.warn('Directory must start with /tmp/ to clobber: %s', tmpdir)
  else:
    try:
      shutil.rmtree(tmpdir)
    except os.error:
      logging.error("Could not delete: %s", tmpdir)


def _XvfbPidFilename(slave_build_name):
  """Returns the filename to the Xvfb pid file.

  This name is unique for each builder.
  This is used by the linux builders.
  """
  return os.path.join(tempfile.gettempdir(), 'xvfb-%s.pid' % slave_build_name)


def StartVirtualX(slave_build_name, build_dir):
  """Start a virtual X server and set the DISPLAY environment variable so sub
  processes will use the virtual X server.  Also start icewm. This only works
  on Linux and assumes that xvfb and icewm are installed.

  Args:
    slave_build_name: The name of the build that we use for the pid file.
        E.g., webkit-rel-linux.
    build_dir: The directory where binaries are produced.  If this is non-empty,
        we try running xdisplaycheck from |build_dir| to verify our X
        connection.
  """
  # We use a pid file to make sure we don't have any xvfb processes running
  # from a previous test run.
  StopVirtualX(slave_build_name)

  # Start a virtual X server that we run the tests in.  This makes it so we can
  # run the tests even if we didn't start the tests from an X session.
  proc = subprocess.Popen(['Xvfb', ':9', '-screen', '0', '1024x768x24', '-ac'],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  xvfb_pid_filename = _XvfbPidFilename(slave_build_name)
  open(xvfb_pid_filename, 'w').write(str(proc.pid))
  os.environ['DISPLAY'] = ':9'

  # Verify that Xvfb has started by using xdisplaycheck.
  if len(build_dir) > 0:
    xdisplaycheck_path = os.path.join(build_dir, 'xdisplaycheck')
    if os.path.exists(xdisplaycheck_path):
      logging.debug('Verifying Xvfb has started...')
      status, output = commands.getstatusoutput(xdisplaycheck_path)
      if status != 0:
        logging.debug('Xvfb return code (None if still running): %s',
                      proc.poll())
        logging.debug('Xvfb stdout and stderr:', proc.communicate())
        raise Exception(output)
      logging.debug('...OK')
  # Some ChromeOS tests need a window manager.
  subprocess.Popen('icewm', stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def StopVirtualX(slave_build_name):
  """Try and stop the virtual X server if one was started with StartVirtualX.

  When the X server dies, it takes down the window manager with it.
  If a virtual x server is not running, this method does nothing.
  """
  xvfb_pid_filename = _XvfbPidFilename(slave_build_name)
  if os.path.exists(xvfb_pid_filename):
    # If the process doesn't exist, we raise an exception that we can ignore.
    try:
      os.kill(int(open(xvfb_pid_filename).read()), signal.SIGKILL)
    except OSError:
      pass
    os.remove(xvfb_pid_filename)


def _svn(cmd):
  """Returns output of given svn command."""
  svn = subprocess.Popen(
      ['svn', '--non-interactive', cmd],
      stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  return svn.communicate()[0]


def _get_value_for_key(lines, key):
  """Given list of |lines| with colon separated key value pairs,
  return the value of |key|."""
  for line in lines:
    parts = line.split(':')
    if parts[0].strip() == key:
      return parts[1].strip()
  return None


def GetCPU():
  # When /proc/cpuinfo exists it is more reliable than platform.
  if os.path.exists('/proc/cpuinfo'):
    with open('/proc/cpuinfo') as f:
      model_name = _get_value_for_key(f.readlines(), 'model name')
      if model_name:
        return model_name
  return platform.processor()


def GetVersion():
  svn_info = _svn('info')
  if svn_info:
    revision = _get_value_for_key(svn_info.split('\n'), 'Revision')
    if revision:
      return revision
  return 'unknown'


class TestInstance:
  def __init__(self, network, log_level, log_file, record,
               diff_unknown_requests, screenshot_dir, cache_miss_file=None,
               use_deterministic_script=False,
               use_chrome_deterministic_js=True,
               use_closest_match=False,
               use_server_delay=False):
    self.network = network
    self.log_level = log_level
    self.log_file = log_file
    self.record = record
    self.proxy_process = None
    self.spdy_proxy_process = None
    self.diff_unknown_requests = diff_unknown_requests
    self.screenshot_dir = screenshot_dir
    self.cache_miss_file = cache_miss_file
    self.use_deterministic_script = use_deterministic_script
    self.use_chrome_deterministic_js = use_chrome_deterministic_js
    self.use_closest_match = use_closest_match
    self.use_server_delay = use_server_delay

  def GenerateConfigFile(self, notes=''):
    # The PerfTracker extension requires this name in order to kick off.
    ext_suffix = 'startbenchmark.html'
    self.filename = tempfile.mktemp(suffix=ext_suffix, prefix='')
    benchmark = {
      'user': getpass.getuser(),
      'notes': str(notes),
      'cmdline': ' '.join(sys.argv),
      'server_url': runner_cfg.appengine_url,
      'server_login': options.login_url,
      'client_hostname': platform.node(),
      'harness_version': GetVersion(),
      'cpu': GetCPU(),
      'iterations': str(runner_cfg.iterations),
      'download_bandwidth_kbps': str(self.network['bandwidth_kbps']['down']),
      'upload_bandwidth_kbps': str(self.network['bandwidth_kbps']['up']),
      'round_trip_time_ms': str(self.network['round_trip_time_ms']),
      'packet_loss_rate': str(self.network['packet_loss_percent']),
      'protocol': self.network['protocol'],
      'urls': runner_cfg.urls,
      'record': self.record,
      'screenshot_dir': self.screenshot_dir,
    }
    with open(self.filename, 'w+') as f:
      f.write("""
<body>
<h3>Running benchmark...</h3>
<script>
// Sometimes the extension doesn't load.
// Set a timer, if the extension didn't write an ACK into the status
// box, then reload this page.
setTimeout(function() {
  var status = document.getElementById("status");
  if (status.textContent != "ACK") {
    console.log("Benchmark stuck?  Reloading.");
    window.location.reload(true);
  }
}, 30000);
</script>
<textarea id=json style="width:100%%;height:80%%;">
%s
</textarea>
<textarea id=status></textarea>
</body>
""" % json.dumps(benchmark, indent=2))


  def StartProxy(self):
    # To run SPDY, we use the SPDY-to-HTTP gateway.  The gateway will answer
    # on port |SERVER_PORT|, contacting the backend server (the replay server)
    # which will run on |BACKEND_SERVER_PORT|.
    port = SERVER_PORT
    init_cwnd = 10
    protocol = self.network['protocol']
    if 'spdy' in protocol:
      port = BACKEND_SERVER_PORT
      init_cwnd = 32

    if protocol == 'http-base':
      init_cwnd = 3   # See RFC3390

    cmdline = [
        REPLAY_PATH,
        '--no-dns_forwarding',
        '--port', str(port),
        '--ssl_port', str(SERVER_PORT_SSL),
        '--shaping_port', str(SERVER_PORT),
        '--log_level', self.log_level,
        '--init_cwnd', str(init_cwnd),
        '--use_closest_match',
        ]
    if self.cache_miss_file:
      cmdline += ['-e', self.cache_miss_file]
    if self.use_closest_match:
      cmdline += ['--use_closest_match']
    if self.use_server_delay:
      cmdline += ['--use_server_delay']
    if not self.use_deterministic_script:
      cmdline += ['--inject_scripts=']
    if self.log_file:
      cmdline += ['--log_file', self.log_file]
    if self.network['bandwidth_kbps']['down']:
      cmdline += ['-d', str(self.network['bandwidth_kbps']['down']) + 'Kbit/s']
    if self.network['bandwidth_kbps']['up']:
      cmdline += ['-u', str(self.network['bandwidth_kbps']['up']) + 'Kbit/s']
    if self.network['round_trip_time_ms']:
      cmdline += ['-m', str(self.network['round_trip_time_ms'])]
    if self.network['packet_loss_percent']:
      cmdline += ['-p', str(self.network['packet_loss_percent'] / 100.0)]
    if not self.diff_unknown_requests:
      cmdline.append('--no-diff_unknown_requests')
    if self.screenshot_dir:
      cmdline += ['-I', self.screenshot_dir]
    if self.record:
      cmdline.append('-r')
    cmdline.append(runner_cfg.replay_data_archive)

    logging.info('Starting Web-Page-Replay: %s', ' '.join(cmdline))
    self.proxy_process = subprocess.Popen(cmdline)
    time.sleep(1)
    assert self.proxy_process.poll() == None

  def StopProxy(self):
    if self.proxy_process:
      logging.debug('Stopping Web-Page-Replay')
      # Use a SIGINT here so that it can do graceful cleanup.
      # Otherwise we'll leave subprocesses hanging.
      self.proxy_process.send_signal(signal.SIGINT)
      self.proxy_process.wait()

  def StartSpdyProxy(self):
    cert_file = ""
    key_file = ""
    protocol = self.network['protocol']
    if protocol == "spdy":
      cert_file = runner_cfg.ssl['certfile']
      key_file = runner_cfg.ssl['keyfile']

    proxy_cfg = "--proxy1=%s" % ",".join((
        "",                        # listen_host
        str(SERVER_PORT),          # listen_port
        cert_file,                 # cert_file
        key_file,                  # key_file
        "127.0.0.1",               # http_host
        str(BACKEND_SERVER_PORT),  # http_port
        "",                        # https_host
        "",                        # https_port
        "0",                       # spdy_only
        ))

    # TODO(mbelshe): Remove the logfile when done with debugging the flipserver.
    logfile = "/tmp/flipserver.log"
    try:
      os.remove(logfile)
    except OSError:
      pass
    cmdline = [
      runner_cfg.spdy_proxy_server_path,
      proxy_cfg,
      "--force_spdy",
      "--v=2",
      "--logfile=" + logfile
    ]
    logging.debug('Starting SPDY proxy: %s', ' '.join(cmdline))
    self.spdy_proxy_process = subprocess.Popen(cmdline,
                                               stdout=subprocess.PIPE,
                                               stderr=subprocess.STDOUT)

  def StopSpdyProxy(self):
    if self.spdy_proxy_process:
      logging.debug('Stopping SPDY Proxy')
      try:
        # For the SPDY server we kill it, because it has no dependencies.
        self.spdy_proxy_process.kill()
      except OSError:
        pass
      self.spdy_proxy_process.wait()

  def RunChrome(self, chrome_cmdline):
    start_file_url = 'file://' + self.filename

    profile_dir = tempfile.mkdtemp(prefix='chrome.profile.')

    use_virtualx = False
    switch_away_from_root = None
    if platform.system() == 'Linux':
      use_virtualx = True
      if os.getuid() == 0:
        user_uid = int(os.getenv('SUDO_UID'))
        switch_away_from_root = lambda: os.setuid(user_uid)
        os.chown(profile_dir, user_uid, -1)

    try:
      if use_virtualx:
        StartVirtualX(platform.node(), '/tmp')

      cmdline = [
          runner_cfg.chrome_path,
          '--activate-on-launch',
          '--disable-background-networking',
          # Stop the translate bar from appearing at the top of the page. When
          # it's there, the screenshots are shorter than they should be.
          '--disable-translate',

          '--enable-benchmarking',
          '--enable-experimental-extension-apis',
          '--enable-logging',
          '--enable-stats-table',
          '--host-resolver-rules=MAP * 127.0.0.1,EXCLUDE %s' % (
              runner_cfg.appengine_host),
          '--ignore-certificate-errors',
          '--load-extension=' + PERFTRACKER_EXTENSION_PATH,
          '--log-level=0',
          '--no-first-run',
          '--no-proxy-server',
          '--start-maximized',
          '--testing-fixed-http-port=' + str(SERVER_PORT),
          '--testing-fixed-https-port=' + str(SERVER_PORT_SSL),
          '--user-data-dir=' + profile_dir,
          ]
      if self.use_chrome_deterministic_js:
        cmdline += ['--no-js-randomness']
      if self.cache_miss_file:
        cmdline += ['--no-sandbox']

      spdy_mode = None
      if self.network['protocol'] == 'spdy':
        spdy_mode = 'ssl'
      if self.network['protocol'] == 'spdy-nossl':
        spdy_mode = 'no-ssl'
      if spdy_mode:
        cmdline.append(
            '--use-spdy=%s,exclude=%s' % (spdy_mode, runner_cfg.appengine_url))
      if chrome_cmdline:
        cmdline.extend(chrome_cmdline.split(' '))
      cmdline.append(start_file_url)

      logging.info('Starting Chrome: %s', ' '.join(cmdline))
      chrome = subprocess.Popen(cmdline, preexec_fn=switch_away_from_root)
      returncode = chrome.wait()
      if returncode:
        logging.error('Chrome returned status code %d. It may have crashed.',
                      returncode)
    finally:
      ClobberTmpDirectory(profile_dir)
      if use_virtualx:
        StopVirtualX(platform.node())

  def RunTest(self, notes, chrome_cmdline):
    try:
      self.GenerateConfigFile(notes)
      self.StartProxy()
      protocol = self.network['protocol']
      if 'spdy' in protocol:
        self.StartSpdyProxy()
      self.RunChrome(chrome_cmdline)
    finally:
      logging.debug('Cleaning up test')
      self.StopProxy()
      self.StopSpdyProxy()
      self.Cleanup()

  def Cleanup(self):
    if os.path.exists(self.filename):
      os.remove(self.filename)


def ConfigureLogging(log_level_name, log_file_name):
  """Configure logging level and format.

  Args:
    log_level_name: 'debug', 'info', 'warning', 'error', or 'critical'.
    log_file_name: a file name
  """
  if logging.root.handlers:
    logging.critical('A logging method (e.g. "logging.warn(...)")'
                     ' was called before logging was configured.')
  log_level = getattr(logging, log_level_name.upper())
  log_format = '%(asctime)s %(levelname)s %(message)s'
  logging.basicConfig(level=log_level, format=log_format)
  if log_file_name:
    fh = logging.FileHandler(log_file_name)
    fh.setLevel(log_level)
    fh.setFormatter(logging.Formatter(log_format))
    logging.getLogger().addHandler(fh)


def main(options, cache_miss_file):
  # When in record mode, override most of the configuration.
  if options.record:
    runner_cfg.replay_data_archive = options.record
    runner_cfg.iterations = 1
    runner_cfg.networks = [
      {
        'bandwidth_kbps': {
          'up': 0,
          'down': 0
        },
        'round_trip_time_ms': 0,
        'packet_loss_percent': 0,
        'protocol': 'http',
      }
    ]

  while True:
    for network in runner_cfg.networks:
      logging.debug("Running network configuration: %s", network)
      test = TestInstance(
          network, options.log_level, options.log_file, options.record,
          options.diff_unknown_requests, options.screenshot_dir,
          cache_miss_file, options.use_deterministic_script,
          options.use_chrome_deterministic_js, options.use_closest_match,
          options.use_server_delay)
      test.RunTest(options.notes, options.chrome_cmdline)
    if not options.infinite or options.record:
      break
    if runner_cfg.inter_run_cleanup_script:
      logging.debug("Running inter-run-cleanup-script")
      subprocess.call([runner_cfg.inter_run_cleanup_script], shell=True)


if __name__ == '__main__':
  log_levels = ('debug', 'info', 'warning', 'error', 'critical')

  class PlainHelpFormatter(optparse.IndentedHelpFormatter):
    def format_description(self, description):
      if description:
        return description + '\n'
      else:
        return ''

  option_parser = optparse.OptionParser(
      usage='%prog -u user',
      formatter=PlainHelpFormatter(),
      description=description,
      epilog='')

  option_parser.add_option('-l', '--log_level', default='error',
      action='store',
      type='choice',
      choices=log_levels,
      help='Minimum verbosity level to log')
  option_parser.add_option('-f', '--log_file', default=None,
      action='store',
      type='string',
      help='Log file to use in addition to writting logs to stderr.')
  option_parser.add_option('-r', '--record', default=False,
      action='store_true',
      dest='do_record',
      help=('Record URLs to file specified by runner_cfg.'))
  option_parser.add_option('-i', '--infinite', default=False,
      action='store_true',
      help='Loop infinitely, repeating the test.')
  option_parser.add_option('-o', '--chrome_cmdline', default=None,
      action='store',
      type='string',
      help='Command line options to pass to chrome.')
  option_parser.add_option('-n', '--notes', default='',
      action='store',
      type='string',
      help='Notes to record with this test run.')
  option_parser.add_option('-u', '--user', default=None,
      action='store',
      type='string',
      help='Username for logging into appengine.')
  option_parser.add_option('-D', '--no-diff_unknown_requests', default=True,
      action='store_false',
      dest='diff_unknown_requests',
      help='During replay, do not show a diff of any unknown requests against '
           'their nearest match in the archive.')
  option_parser.add_option('-I', '--screenshot_dir', default=None,
      action='store',
      type='string',
      help='Save PNG images of the loaded page in the given directory.')
  option_parser.add_option('-d', '--deterministic_script', default=False,
      action='store_true',
      dest='use_deterministic_script',
      help='During a record, inject JavaScript to make sources of '
           'entropy such as Date() and Math.random() deterministic. CAUTION: '
           'Without this option many web pages will not replay properly.')
  option_parser.add_option('-j', '--no_chrome_deterministic_js', default=True,
      action='store_false',
      dest='use_chrome_deterministic_js',
      help='Enable Chrome\'s deterministic implementations of javascript.'
           'This makes sources of entropy such as Date() and Math.random()'
           'deterministic.')
  option_parser.add_option('-e', '--cache_miss_file', default=None,
      action='store',
      dest='cache_miss_file',
      type='string',
      help='Archive file to record cache misses in replay mode.')
  option_parser.add_option('-C', '--use_closest_match', default=False,
      action='store_true',
      dest='use_closest_match',
      help='During replay, if a request is not found, serve the closest match'
           'in the archive instead of giving a 404.')
  option_parser.add_option('-U', '--use_server_delay', default=False,
      action='store_true',
      dest='use_server_delay',
      help='During replay, simulate server delay by delaying response time to'
           'requests.')


  options, args = option_parser.parse_args()

  ConfigureLogging(options.log_level, options.log_file)

  # Collect login credentials and verify
  user = options.user or runner_cfg.appengine_user
  if user:
    password = (runner_cfg.appengine_password
                or getpass.getpass(user + ' password: '))
    options.login_url = DoAppEngineLogin(user, password)
    if not options.login_url:
      exit(-1)
  elif runner_cfg.appengine_host != 'localhost':
    option_parser.error('Must specify an appengine user for login')
    exit(-1)
  else:
    options.login_url = ''

  # run the recording round, if specified
  if options.do_record:
    logging.debug("Running on record mode")
    options.record = runner_cfg.replay_data_archive
    main(options, options.cache_miss_file)
    options.do_record = False

  options.record = None
  # run the replay round
  logging.debug("Running on replay mode")
  sys.exit(main(options, options.cache_miss_file))
