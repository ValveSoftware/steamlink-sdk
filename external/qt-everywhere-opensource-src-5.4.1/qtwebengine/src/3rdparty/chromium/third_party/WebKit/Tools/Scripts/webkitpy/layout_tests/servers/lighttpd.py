# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Start and stop the lighttpd server as it is used by the layout tests."""

import logging
import time

from webkitpy.layout_tests.servers import server_base


_log = logging.getLogger(__name__)


class Lighttpd(server_base.ServerBase):

    def __init__(self, port_obj, output_dir):
        super(Lighttpd, self).__init__(port_obj, output_dir)
        self._name = 'lighttpd'
        self._log_prefixes = ('access.log-', 'error.log-')
        self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)

        self._layout_tests_dir = self._port_obj.layout_tests_dir()

        self._webkit_tests = self._filesystem.join(self._layout_tests_dir, 'http', 'tests')
        self._js_test_resource = self._filesystem.join(self._layout_tests_dir, 'resources')
        self._media_resource = self._filesystem.join(self._layout_tests_dir, 'media')

        # Self generated certificate for SSL server.
        self._pem_file = self._filesystem.join(self._layout_tests_dir, 'http', 'conf', 'httpd2.pem')

        self.mappings = [
            {'port': 8000, 'docroot': self._webkit_tests},
            {'port': 8080, 'docroot': self._webkit_tests},
            {'port': 8443, 'docroot': self._webkit_tests, 'sslcert': self._pem_file},
        ]

        self._start_cmd = [
            self._port_obj.path_to_lighttpd(),
            '-f', self._filesystem.join(self._output_dir, 'lighttpd.conf'),
            '-m', self._port_obj.path_to_lighttpd_modules(),
            '-D',
        ]
        self._env = self._port_obj.setup_environ_for_server()

    def _prepare_config(self):
        time_str = time.strftime("%d%b%Y-%H%M%S")
        access_file_name = "access.log-" + time_str + ".txt"
        access_log = self._filesystem.join(self._output_dir, access_file_name)
        log_file_name = "error.log-" + time_str + ".txt"
        error_log = self._filesystem.join(self._output_dir, log_file_name)

        # Write out the config
        base_conf_file = self._filesystem.join(self._layout_tests_dir, 'http', 'conf', 'lighttpd.conf')
        out_conf_file = self._filesystem.join(self._output_dir, 'lighttpd.conf')
        with self._filesystem.open_text_file_for_writing(out_conf_file) as f:
            base_conf = self._filesystem.read_text_file(base_conf_file)
            f.write(base_conf)

            # Write out our cgi handlers.  Run perl through env so that it
            # processes the #! line and runs perl with the proper command
            # line arguments. Emulate apache's mod_asis with a cat cgi handler.
            f.write(('cgi.assign = ( ".cgi"  => "/usr/bin/env",\n'
                    '               ".pl"   => "/usr/bin/env",\n'
                    '               ".asis" => "/bin/cat",\n'
                    '               ".php"  => "%s" )\n\n') %
                                        self._port_obj.path_to_lighttpd_php())

            # Setup log files
            f.write(('server.errorlog = "%s"\n'
                    'accesslog.filename = "%s"\n\n') % (error_log, access_log))

            # Setup upload folders. Upload folder is to hold temporary upload files
            # and also POST data. This is used to support XHR layout tests that
            # does POST.
            f.write(('server.upload-dirs = ( "%s" )\n\n') % (self._output_dir))

            # Setup a link to where the js test templates are stored
            f.write(('alias.url = ( "/js-test-resources" => "%s" )\n\n') %
                        (self._js_test_resource))

            # Setup a link to where the media resources are stored.
            f.write(('alias.url += ( "/media-resources" => "%s" )\n\n') %
                        (self._media_resource))

            # dump out of virtual host config at the bottom.
            for mapping in self.mappings:
                ssl_setup = ''
                if 'sslcert' in mapping:
                    ssl_setup = ('  ssl.engine = "enable"\n'
                                '  ssl.pemfile = "%s"\n' % mapping['sslcert'])

                f.write(('$SERVER["socket"] == "127.0.0.1:%d" {\n'
                        '  server.document-root = "%s"\n' +
                        ssl_setup +
                        '}\n\n') % (mapping['port'], mapping['docroot']))

        # Copy liblightcomp.dylib to /tmp/lighttpd/lib to work around the
        # bug that mod_alias.so loads it from the hard coded path.
        if self._port_obj.host.platform.is_mac():
            tmp_module_path = '/tmp/lighttpd/lib'
            if not self._filesystem.exists(tmp_module_path):
                self._filesystem.maybe_make_directory(tmp_module_path)
            lib_file = 'liblightcomp.dylib'
            self._filesystem.copyfile(self._filesystem.join(self._port_obj.path_to_lighttpd_modules(), lib_file),
                                      self._filesystem.join(tmp_module_path, lib_file))
