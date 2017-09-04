# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

"""Webkit-patch is a tool with multiple sub-commands with different purposes.

Historically, it had commands related to dealing with bugzilla, and posting
and comitting patches to WebKit. More recently, it has commands for printing
expectations, fetching new test baselines, starting a commit-announcer IRC bot,
etc. These commands don't necessarily have anything to do with each other.
"""

import logging
import optparse
import sys

from webkitpy.common.host import Host
from webkitpy.tool.commands.analyze_baselines import AnalyzeBaselines
from webkitpy.tool.commands.auto_rebaseline import AutoRebaseline
from webkitpy.tool.commands.command import HelpPrintingOptionParser
from webkitpy.tool.commands.commit_announcer import CommitAnnouncerCommand
from webkitpy.tool.commands.flaky_tests import FlakyTests
from webkitpy.tool.commands.help_command import HelpCommand
from webkitpy.tool.commands.layout_tests_server import LayoutTestsServer
from webkitpy.tool.commands.optimize_baselines import OptimizeBaselines
from webkitpy.tool.commands.pretty_diff import PrettyDiff
from webkitpy.tool.commands.queries import CrashLog
from webkitpy.tool.commands.queries import PrintBaselines
from webkitpy.tool.commands.queries import PrintExpectations
from webkitpy.tool.commands.rebaseline import CopyExistingBaselinesInternal
from webkitpy.tool.commands.rebaseline import Rebaseline
from webkitpy.tool.commands.rebaseline import RebaselineExpectations
from webkitpy.tool.commands.rebaseline import RebaselineJson
from webkitpy.tool.commands.rebaseline import RebaselineTest
from webkitpy.tool.commands.rebaseline_cl import RebaselineCL
from webkitpy.tool.commands.rebaseline_server import RebaselineServer


_log = logging.getLogger(__name__)


class WebKitPatch(Host):
    global_options = [
        optparse.make_option(
            "-v", "--verbose", action="store_true", dest="verbose", default=False,
            help="enable all logging"),
        optparse.make_option(
            "-d", "--directory", action="append", dest="patch_directories", default=[],
            help="Directory to look at for changed files"),
    ]

    def __init__(self, path):
        super(WebKitPatch, self).__init__()
        self._path = path
        self.commands = [
            AnalyzeBaselines(),
            AutoRebaseline(),
            CommitAnnouncerCommand(),
            CopyExistingBaselinesInternal(),
            CrashLog(),
            FlakyTests(),
            LayoutTestsServer(),
            OptimizeBaselines(),
            PrettyDiff(),
            PrintBaselines(),
            PrintExpectations(),
            Rebaseline(),
            RebaselineCL(),
            RebaselineExpectations(),
            RebaselineJson(),
            RebaselineServer(),
            RebaselineTest(),
        ]
        self.help_command = HelpCommand(tool=self)
        self.commands.append(self.help_command)

    def main(self, argv=None):
        argv = argv or sys.argv
        (command_name, args) = self._split_command_name_from_args(argv[1:])

        option_parser = self._create_option_parser()
        self._add_global_options(option_parser)

        command = self.command_by_name(command_name) or self.help_command
        if not command:
            option_parser.error("%s is not a recognized command", command_name)

        command.set_option_parser(option_parser)
        (options, args) = command.parse_args(args)
        self._handle_global_options(options)

        (should_execute, failure_reason) = self._should_execute_command(command)
        if not should_execute:
            _log.error(failure_reason)
            return 0  # FIXME: Should this really be 0?

        result = command.check_arguments_and_execute(options, args, self)
        return result

    def path(self):
        return self._path

    @staticmethod
    def _split_command_name_from_args(args):
        # Assume the first argument which doesn't start with "-" is the command name.
        command_index = 0
        for arg in args:
            if arg[0] != "-":
                break
            command_index += 1
        else:
            return (None, args[:])

        command = args[command_index]
        return (command, args[:command_index] + args[command_index + 1:])

    def _create_option_parser(self):
        usage = "Usage: %prog [options] COMMAND [ARGS]"
        name = optparse.OptionParser().get_prog_name()
        return HelpPrintingOptionParser(epilog_method=self.help_command.help_epilog, prog=name, usage=usage)

    def _add_global_options(self, option_parser):
        global_options = self.global_options or []
        for option in global_options:
            option_parser.add_option(option)

    # FIXME: This may be unnecessary since we pass global options to all commands during execute() as well.
    def _handle_global_options(self, options):
        self.initialize_scm(options.patch_directories)

    def _should_execute_command(self, command):
        if command.requires_local_commits and not self.scm().supports_local_commits():
            failure_reason = "%s requires local commits using %s in %s." % (
                command.name, self.scm().display_name(), self.scm().checkout_root)
            return (False, failure_reason)
        return (True, None)

    def name(self):
        return optparse.OptionParser().get_prog_name()

    def should_show_in_main_help(self, command):
        if not command.show_in_main_help:
            return False
        if command.requires_local_commits:
            return self.scm().supports_local_commits()
        return True

    def command_by_name(self, command_name):
        for command in self.commands:
            if command_name == command.name:
                return command
        return None
