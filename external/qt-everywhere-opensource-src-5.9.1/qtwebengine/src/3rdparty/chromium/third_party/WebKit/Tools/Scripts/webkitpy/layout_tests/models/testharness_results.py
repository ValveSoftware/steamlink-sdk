# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility module for checking testharness test output."""

_TESTHARNESSREPORT_HEADER = 'This is a testharness.js-based test.'
_TESTHARNESSREPORT_FOOTER = 'Harness: the test ran to completion.'


def is_all_pass_testharness_result(content_text):
    """Returns whether |content_text| is a testharness result that only contains PASS lines."""
    return (is_testharness_output(content_text) and
            is_testharness_output_passing(content_text) and
            not has_console_errors_or_warnings(content_text))


def is_testharness_output(content_text):
    """Returns whether |content_text| is a testharness output."""
    # Leading and trailing white spaces are accepted.
    lines = content_text.strip().splitlines()
    lines = [line.strip() for line in lines]

    # A testharness output is defined as containing the header and the footer.
    found_header = False
    found_footer = False
    for line in lines:
        if line == _TESTHARNESSREPORT_HEADER:
            found_header = True
        elif line == _TESTHARNESSREPORT_FOOTER:
            found_footer = True

    return found_header and found_footer


def is_testharness_output_passing(content_text):
    """Returns whether |content_text| is a passing testharness output."""

    # Leading and trailing white spaces are accepted.
    lines = content_text.strip().splitlines()
    lines = [line.strip() for line in lines]

    # The check is very conservative and rejects any unexpected content in the output.
    previous_line_is_console_line = False
    for line in lines:
        # There should be no empty lines, unless the empty line follows a console message.
        if len(line) == 0:
            if previous_line_is_console_line:
                continue
            else:
                return False

        # Those lines are expected to be exactly equivalent.
        if line == _TESTHARNESSREPORT_HEADER or line == _TESTHARNESSREPORT_FOOTER:
            previous_line_is_console_line = False
            continue

        # Those are expected passing output.
        if line.startswith('CONSOLE'):
            previous_line_is_console_line = True
            continue

        if line.startswith('PASS'):
            previous_line_is_console_line = False
            continue

        # Those are expected failing output.
        if (line.startswith('FAIL') or
                line.startswith('TIMEOUT') or
                line.startswith('NOTRUN') or
                line.startswith('Harness Error. harness_status = ')):
            return False

        # Unexpected output should be considered as a failure.
        return False

    return True


def has_console_errors_or_warnings(content_text):
    """Returns whether |content_text| is has console errors or warnings."""

    def is_warning_or_error(line):
        return line.startswith('CONSOLE ERROR:') or line.startswith('CONSOLE WARNING:')

    lines = content_text.strip().splitlines()
    return any(is_warning_or_error(line) for line in lines)
