#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the test suite module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:BSD$
## You may use this file under the terms of the BSD license as follows:
##
## "Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##   * Redistributions of source code must retain the above copyright
##     notice, this list of conditions and the following disclaimer.
##   * Redistributions in binary form must reproduce the above copyright
##     notice, this list of conditions and the following disclaimer in
##     the documentation and/or other materials provided with the
##     distribution.
##   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
##     of its contributors may be used to endorse or promote products derived
##     from this software without specific prior written permission.
##
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
##
## $QT_END_LICENSE$
##
#############################################################################

# Copyright 2009 the Sputnik authors.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

# This is derived from sputnik.py, the Sputnik console test runner,
# with elements from packager.py, which is separately
# copyrighted. TODO: Refactor so there is less duplication between
# test262.py and packager.py.

import sys
from os import path
rootDir = path.dirname(path.realpath(__file__))
sys.path.insert(0, path.abspath(rootDir + "/test262/tools/packaging"))

import logging
import optparse
import os
import platform
import re
import subprocess
import tempfile
import time
import xml.dom.minidom
import datetime
import shutil
import json
import stat
import multiprocessing
import signal


from parseTestRecord import parseTestRecord, stripHeader

from packagerConfig import *

# ############# Helpers needed for parallel multi-process test execution ############

def runTest(case, args):
    return case.Run(args)

def runTestVarArgs(args):
    return runTest(*args)

def initWorkerProcess():
    signal.signal(signal.SIGINT, signal.SIG_IGN)

# #############

class Test262Error(Exception):
  def __init__(self, message):
    self.message = message

def ReportError(s):
  raise Test262Error(s)


class TestExpectations:
    def __init__(self, enabled):
        self.testsToSkip = []
        self.failingTests = []
        f = open(rootDir + "/TestExpectations")
        if not enabled:
            return
        for line in f.read().splitlines():
            line = line.strip()
            if len(line) == 0 or line[0] == "#":
                continue
            record = line.split()
            if len(record) == 1:
                self.testsToSkip.append(record[0])
            else:
                test = record[0]
                expectation = record[1]
                if expectation == "failing":
                    self.failingTests.append(test)
        f.close()

    def update(self, progress):
        unexpectedPasses = [c.case.name[-1] for c in progress.failed_tests if c.case.IsNegative()]

        # If a test fails that we expected to fail, then it actually passed unexpectedly.
        failures = [c.case.name[-1] for c in progress.failed_tests if not c.case.IsNegative()]
        for failure in failures:
            if failure in self.failingTests:
                unexpectedPasses.append(failure)

        f = open(rootDir + "/TestExpectations")
        lines = f.read().splitlines()
        oldLen = len(lines)
        for result in unexpectedPasses:
            expectationLine = result + " failing"
            try:
                lines.remove(expectationLine)
            except ValueError:
                pass

        f.close()
        if len(lines) != oldLen:
            f = open(rootDir + "/TestExpectations", "w")
            f.write("\n".join(lines))
            f.close()
            print "Changes to TestExpectations written!"


if not os.path.exists(EXCLUDED_FILENAME):
    print "Cannot generate (JSON) test262 tests without a file," + \
        " %s, showing which tests have been disabled!" % EXCLUDED_FILENAME
    sys.exit(1)
EXCLUDE_LIST = xml.dom.minidom.parse(EXCLUDED_FILENAME)
EXCLUDE_LIST = EXCLUDE_LIST.getElementsByTagName("test")
EXCLUDE_LIST = [x.getAttribute("id") for x in EXCLUDE_LIST]


def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("--command", default="qmljs", help="The command-line to run")
  result.add_option("--tests", default=path.abspath(rootDir + '/test262'),
                    help="Path to the tests")
  result.add_option("--cat", default=False, action="store_true",
                    help="Print packaged test code that would be run")
  result.add_option("--summary", default=True, action="store_true",
                    help="Print summary after running tests")
  result.add_option("--full-summary", default=False, action="store_true",
                    help="Print summary and test output after running tests")
  result.add_option("--strict_only", default=False, action="store_true",
                    help="Test only strict mode")
  result.add_option("--non_strict_only", default=False, action="store_true",
                    help="Test only non-strict mode")
  result.add_option("--parallel", default=False, action="store_true",
                    help="Run tests in parallel")
  result.add_option("--with-test-expectations", default=False, action="store_true",
                    help="Parse TestExpectations to deal with tests known to fail")
  result.add_option("--update-expectations", default=False, action="store_true",
                    help="Update test expectations fail when a test passes that was expected to fail")
  # TODO: Once enough tests are made strict compat, change the default
  # to "both"
  result.add_option("--unmarked_default", default="non_strict",
                    help="default mode for tests of unspecified strictness")
  return result


def ValidateOptions(options):
  if not options.command:
    ReportError("A --command must be specified.")
  if not path.exists(options.tests):
    ReportError("Couldn't find test path '%s'" % options.tests)


placeHolderPattern = re.compile(r"\{\{(\w+)\}\}")


def IsWindows():
  p = platform.system()
  return (p == 'Windows') or (p == 'Microsoft')


class TempFile(object):

  def __init__(self, suffix="", prefix="tmp", text=False):
    self.suffix = suffix
    self.prefix = prefix
    self.text = text
    self.fd = None
    self.name = None
    self.is_closed = False
    self.Open()

  def Open(self):
    (self.fd, self.name) = tempfile.mkstemp(
        suffix = self.suffix,
        prefix = self.prefix,
        text = self.text)

  def Write(self, str):
    os.write(self.fd, str)

  def Read(self):
    f = file(self.name)
    result = f.read()
    f.close()
    return result

  def Close(self):
    if not self.is_closed:
      self.is_closed = True
      os.close(self.fd)

  def Dispose(self):
    try:
      self.Close()
      os.unlink(self.name)
    except OSError, e:
      logging.error("Error disposing temp file: %s", str(e))


class TestResult(object):

  def __init__(self, exit_code, stdout, stderr, case):
    self.exit_code = exit_code
    self.stdout = stdout
    self.stderr = stderr
    self.case = case

  def ReportOutcome(self, long_format):
    name = self.case.GetName()
    mode = self.case.GetMode()
    if self.HasUnexpectedOutcome():
      if self.case.IsNegative():
        print "=== %s was expected to fail in %s, but didn't ===" % (name, mode)
      else:
        if long_format:
          print "=== %s failed in %s ===" % (name, mode)
        else:
          print "%s in %s: " % (name, mode)
        out = self.stdout.strip()
        if len(out) > 0:
          print "--- output ---"
          print out
        err = self.stderr.strip()
        if len(err) > 0:
          print "--- errors ---"
          print err
        if long_format:
          print "==="
    elif self.case.IsNegative():
      print "%s failed in %s as expected" % (name, mode)
    else:
      print "%s passed in %s" % (name, mode)

  def HasFailed(self):
    return self.exit_code != 0

  def HasUnexpectedOutcome(self):
    if self.case.IsNegative():
       return not self.HasFailed()
    else:
       return self.HasFailed()


class TestCase(object):

  def __init__(self, suite, name, full_path, strict_mode):
    self.suite = suite
    self.name = name
    self.full_path = full_path
    self.strict_mode = strict_mode
    f = open(self.full_path)
    self.contents = f.read()
    f.close()
    testRecord = parseTestRecord(self.contents, name)
    self.test = testRecord["test"]
    del testRecord["test"]
    del testRecord["header"]
    del testRecord["commentary"]
    self.testRecord = testRecord;


  def GetName(self):
    return path.join(*self.name)

  def GetMode(self):
    if self.strict_mode:
      return "strict mode"
    else:
      return "non-strict mode"

  def GetPath(self):
    return self.name

  def NegateResult(self):
      if self.IsNegative():
          del self.testRecord['negative']
      else:
          self.testRecord['negative'] = "Some failure";

  def IsNegative(self):
    return 'negative' in self.testRecord

  def IsOnlyStrict(self):
    return 'onlyStrict' in self.testRecord

  def IsNoStrict(self):
    return 'noStrict' in self.testRecord

  def GetSource(self):
    # "var testDescrip = " + str(self.testRecord) + ';\n\n' + \
    source = self.suite.GetInclude("cth.js") + \
        self.suite.GetInclude("sta.js") + \
        self.suite.GetInclude("ed.js") + \
        self.suite.GetInclude("testBuiltInObject.js") + \
        self.suite.GetInclude("testIntl.js") + \
        self.test + '\n'

    if self.strict_mode:
      source = '"use strict";\nvar strict_mode = true;\n' + source
    else:
      source =  "var strict_mode = false; \n" + source
    return source

  def InstantiateTemplate(self, template, params):
    def GetParameter(match):
      key = match.group(1)
      return params.get(key, match.group(0))
    return placeHolderPattern.sub(GetParameter, template)

  def Execute(self, command):
    if IsWindows():
      args = '%s' % command
    else:
      args = command.split(" ")
    stdout = TempFile(prefix="test262-out-")
    stderr = TempFile(prefix="test262-err-")
    try:
      logging.info("exec: %s", str(args))
      process = subprocess.Popen(
        args,
        shell = IsWindows(),
        stdout = stdout.fd,
        stderr = stderr.fd
      )
      code = process.wait()
      out = stdout.Read()
      err = stderr.Read()
    finally:
      stdout.Dispose()
      stderr.Dispose()
    return (code, out, err)

  def RunTestIn(self, command_template, tmp):
    tmp.Write(self.GetSource())
    tmp.Close()
    command = self.InstantiateTemplate(command_template, {
      'path': tmp.name
    })
    (code, out, err) = self.Execute(command)
    return TestResult(code, out, err, self)

  def Run(self, command_template):
    tmp = TempFile(suffix=".js", prefix="test262-", text=True)
    try:
      result = self.RunTestIn(command_template, tmp)
    finally:
      tmp.Dispose()
    return result

  def Print(self):
    print self.GetSource()


class ProgressIndicator(object):

  def __init__(self, count):
    self.count = count
    self.succeeded = 0
    self.failed = 0
    self.failed_tests = []

  def HasRun(self, result):
    result.ReportOutcome(True)
    if result.HasUnexpectedOutcome():
      self.failed += 1
      self.failed_tests.append(result)
    else:
      self.succeeded += 1


def MakePlural(n):
  if (n == 1):
    return (n, "")
  else:
    return (n, "s")


class TestSuite(object):

  def __init__(self, root, strict_only, non_strict_only, unmarked_default, load_expectations):
    # TODO: derive from packagerConfig.py
    self.test_root = path.join(root, 'test', 'suite')
    self.lib_root = path.join(root, 'test', 'harness')
    self.strict_only = strict_only
    self.non_strict_only = non_strict_only
    self.unmarked_default = unmarked_default
    self.include_cache = { }
    self.expectations = TestExpectations(load_expectations)

  def Validate(self):
    if not path.exists(self.test_root):
      ReportError("No test repository found")
    if not path.exists(self.lib_root):
      ReportError("No test library found")

  def IsHidden(self, path):
    return path.startswith('.') or path == 'CVS'

  def IsTestCase(self, path):
    return path.endswith('.js')

  def ShouldRun(self, rel_path, tests):
    if len(tests) == 0:
      return True
    for test in tests:
      if test in rel_path:
        return True
    return False

  def GetInclude(self, name):
    if not name in self.include_cache:
      static = path.join(self.lib_root, name)
      if path.exists(static):
        f = open(static)
        contents = stripHeader(f.read())
        contents = re.sub(r'\r\n', '\n', contents)
        self.include_cache[name] = contents + "\n"
        f.close()
      else:
        ReportError("Can't find: " + static)
    return self.include_cache[name]

  def EnumerateTests(self, tests):
    logging.info("Listing tests in %s", self.test_root)
    cases = []
    for root, dirs, files in os.walk(self.test_root):
      for f in [x for x in dirs if self.IsHidden(x)]:
        dirs.remove(f)
      dirs.sort()
      for f in sorted(files):
        if self.IsTestCase(f):
          full_path = path.join(root, f)
          if full_path.startswith(self.test_root):
            rel_path = full_path[len(self.test_root)+1:]
          else:
            logging.warning("Unexpected path %s", full_path)
            rel_path = full_path
          if self.ShouldRun(rel_path, tests) and not rel_path.startswith("intl402" + os.sep):
            basename = path.basename(full_path)[:-3]
            name = rel_path.split(path.sep)[:-1] + [basename]
            if EXCLUDE_LIST.count(basename) >= 1 or self.expectations.testsToSkip.count(basename) >= 1:
              print 'Excluded: ' + basename
            else:
              if not self.non_strict_only:
                strict_case = TestCase(self, name, full_path, True)
                if self.expectations.failingTests.count(basename) >= 1:
                    strict_case.NegateResult()
                if not strict_case.IsNoStrict():
                  if strict_case.IsOnlyStrict() or \
                        self.unmarked_default in ['both', 'strict']:
                    cases.append(strict_case)
              if not self.strict_only:
                non_strict_case = TestCase(self, name, full_path, False)
                if self.expectations.failingTests.count(basename) >= 1:
                    non_strict_case.NegateResult()
                if not non_strict_case.IsOnlyStrict():
                  if non_strict_case.IsNoStrict() or \
                        self.unmarked_default in ['both', 'non_strict']:
                    cases.append(non_strict_case)
    logging.info("Done listing tests")
    return cases

  def PrintSummary(self, progress):
    print
    print "=== Summary ==="
    count = progress.count
    succeeded = progress.succeeded
    failed = progress.failed
    print " - Ran %i test%s" % MakePlural(count)
    if progress.failed == 0:
      print " - All tests succeeded"
    else:
      percent = ((100.0 * succeeded) / count,)
      print " - Passed %i test%s (%.1f%%)" % (MakePlural(succeeded) + percent)
      percent = ((100.0 * failed) / count,)
      print " - Failed %i test%s (%.1f%%)" % (MakePlural(failed) + percent)
      positive = [c for c in progress.failed_tests if not c.case.IsNegative()]
      negative = [c for c in progress.failed_tests if c.case.IsNegative()]
      if len(positive) > 0:
        print
        print "Failed tests"
        for result in positive:
          print "  %s in %s" % (result.case.GetName(), result.case.GetMode())
      if len(negative) > 0:
        print
        print "Expected to fail but passed ---"
        for result in negative:
          print " %s in %s" % (result.case.GetName(), result.case.GetMode())

  def PrintFailureOutput(self, progress):
    for result in progress.failed_tests:
      print
      result.ReportOutcome(False)

  def Run(self, command_template, tests, print_summary, full_summary, parallel, update_expectations):
    if not "{{path}}" in command_template:
      command_template += " {{path}}"
    cases = self.EnumerateTests(tests)
    if len(cases) == 0:
      ReportError("No tests to run")
    progress = ProgressIndicator(len(cases))

    if parallel:
      pool = multiprocessing.Pool(processes=multiprocessing.cpu_count(), initializer=initWorkerProcess)
      results = pool.imap_unordered(func=runTestVarArgs, iterable=[(case, command_template) for case in cases], chunksize=multiprocessing.cpu_count() * 8)
      for result in results:
        progress.HasRun(result)
    else:
      for case in cases:
        result = case.Run(command_template)
        progress.HasRun(result)
    if print_summary:
      self.PrintSummary(progress)
      if full_summary:
        self.PrintFailureOutput(progress)
      else:
        print
        print "Use --full-summary to see output from failed tests"
    print
    if update_expectations:
        self.expectations.update(progress)

  def Print(self, tests):
    cases = self.EnumerateTests(tests)
    if len(cases) > 0:
      cases[0].Print()


def Main():
  # Some date tests rely on being run in pacific time.
  # Uncomment the next line for more logging info.
  #logging.basicConfig(level=logging.DEBUG)
  os.environ["TZ"] = "PST8PDT"
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  ValidateOptions(options)
  test_suite = TestSuite(options.tests,
                         options.strict_only,
                         options.non_strict_only,
                         options.unmarked_default,
                         options.with_test_expectations)
  test_suite.Validate()
  if options.cat:
    test_suite.Print(args)
  else:
    test_suite.Run(options.command, args,
                   options.summary or options.full_summary,
                   options.full_summary,
                   options.parallel,
                   options.update_expectations)


if __name__ == '__main__':
  try:
    Main()
    sys.exit(0)
  except Test262Error, e:
    print "Error: %s" % e.message
    sys.exit(1)
