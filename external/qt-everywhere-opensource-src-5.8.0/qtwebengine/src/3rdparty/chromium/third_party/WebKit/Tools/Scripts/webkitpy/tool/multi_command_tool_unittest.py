# Copyright (c) 2009 Google Inc. All rights reserved.
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

import unittest

from optparse import make_option

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.multi_command_tool import MultiCommandTool
from webkitpy.tool.commands.command import Command


class TrivialCommand(Command):
    name = "trivial"
    help_text = "help text"
    long_help = "trivial command long help text"
    show_in_main_help = True

    def __init__(self, **kwargs):
        super(TrivialCommand, self).__init__(**kwargs)

    def execute(self, options, args, tool):
        pass


class UncommonCommand(TrivialCommand):
    name = "uncommon"
    show_in_main_help = False


class TrivialTool(MultiCommandTool):

    def __init__(self, commands):
        MultiCommandTool.__init__(self, commands)

    def name(self):
        return 'trivial-tool'

    def path(self):
        return __file__

    def should_execute_command(self, command):
        return (True, None)


class MultiCommandToolTest(unittest.TestCase):

    def _assert_split(self, args, expected_split):
        self.assertEqual(MultiCommandTool._split_command_name_from_args(args), expected_split)

    def test_split_args(self):
        # MultiCommandToolTest._split_command_name_from_args returns: (command, args)
        full_args = ["--global-option", "command", "--option", "arg"]
        full_args_expected = ("command", ["--global-option", "--option", "arg"])
        self._assert_split(full_args, full_args_expected)

        full_args = []
        full_args_expected = (None, [])
        self._assert_split(full_args, full_args_expected)

        full_args = ["command", "arg"]
        full_args_expected = ("command", ["arg"])
        self._assert_split(full_args, full_args_expected)

    def test_command_by_name(self):
        tool = TrivialTool(commands=[TrivialCommand(), UncommonCommand()])
        self.assertEqual(tool.command_by_name("trivial").name, "trivial")
        self.assertIsNone(tool.command_by_name("bar"))

    def _assert_tool_main_outputs(self, tool, main_args, expected_stdout, expected_stderr="", expected_exit_code=0):
        exit_code = OutputCapture().assert_outputs(
            self, tool.main, [main_args], expected_stdout=expected_stdout, expected_stderr=expected_stderr)
        self.assertEqual(exit_code, expected_exit_code)

    def test_global_help(self):
        tool = TrivialTool(commands=[TrivialCommand(), UncommonCommand()])
        expected_common_commands_help = """Usage: trivial-tool [options] COMMAND [ARGS]

Options:
  -h, --help  show this help message and exit

Common trivial-tool commands:
   trivial   help text

See 'trivial-tool help --all-commands' to list all commands.
See 'trivial-tool help COMMAND' for more information on a specific command.

"""
        self._assert_tool_main_outputs(tool, ["tool"], expected_common_commands_help)
        self._assert_tool_main_outputs(tool, ["tool", "help"], expected_common_commands_help)
        expected_all_commands_help = """Usage: trivial-tool [options] COMMAND [ARGS]

Options:
  -h, --help  show this help message and exit

All trivial-tool commands:
   help       Display information about this program or its subcommands
   trivial    help text
   uncommon   help text

See 'trivial-tool help --all-commands' to list all commands.
See 'trivial-tool help COMMAND' for more information on a specific command.

"""
        self._assert_tool_main_outputs(tool, ["tool", "help", "--all-commands"], expected_all_commands_help)
        # Test that arguments can be passed before commands as well
        self._assert_tool_main_outputs(tool, ["tool", "--all-commands", "help"], expected_all_commands_help)

    def test_command_help(self):
        command_with_options = TrivialCommand(options=[make_option("--my_option")])
        tool = TrivialTool(commands=[command_with_options])
        expected_subcommand_help = """trivial [options]   help text

trivial command long help text

Options:
  --my_option=MY_OPTION

"""
        self._assert_tool_main_outputs(tool, ["tool", "help", "trivial"], expected_subcommand_help)

    def test_constructor_calls_bind_to_tool(self):
        tool = TrivialTool(commands=[TrivialCommand(), UncommonCommand()])
        self.assertEqual(tool.commands[0]._tool, tool)
        self.assertEqual(tool.commands[1]._tool, tool)
