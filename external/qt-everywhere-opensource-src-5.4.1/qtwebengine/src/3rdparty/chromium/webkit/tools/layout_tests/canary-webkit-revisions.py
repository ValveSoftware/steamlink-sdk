#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Retrieve passing and failing WebKit revision numbers from canaries.

From each canary,
- the last WebKit revision number for which all the tests have passed,
- the last WebKit revision number for which the tests were run, and
- the names of failing layout tests
are retrieved and printed.
"""


import json
import optparse
import re
import sys
import urllib2

_WEBKIT_REVISION_IN_DEPS_RE = re.compile(r'"webkit_revision"\s*:\s*"(\d+)"')
_DEPS_FILE_URL = "http://src.chromium.org/viewvc/chrome/trunk/src/DEPS"
_DEFAULT_BUILDERS = [
  "Webkit Win",
  "Webkit Vista",
  "Webkit Win7",
  "Webkit Win (dbg)(1)",
  "Webkit Win (dbg)(2)",
  "Webkit Mac10.5 (CG)",
  "Webkit Mac10.6 (CG)",
  "Webkit Mac10.5 (CG)(dbg)(1)",
  "Webkit Mac10.5 (CG)(dbg)(2)",
  "Webkit Mac10.6 (CG)(dbg)",
  "Webkit Linux",
  "Webkit Linux 32",
  "Webkit Linux (dbg)(1)",
  "Webkit Linux (dbg)(2)",
]
_DEFAULT_MAX_BUILDS = 10
_TEST_PREFIX = "&tests="
_TEST_SUFFIX = '">'
_WEBKIT_TESTS = "webkit_tests"


def _OpenUrl(url):
  """Opens a URL.

  Returns:
      A file-like object in case of success, an empty list otherwise.
  """
  try:
    return urllib2.urlopen(url)
  except urllib2.URLError, url_error:
    message = ""
    # Surprisingly, urllib2.URLError has different attributes based on the
    # kinds of errors -- "code" for HTTP-level errors, "reason" for others.
    if hasattr(url_error, "code"):
      message = "Status code: %d" % url_error.code
    if hasattr(url_error, "reason"):
      message = url_error.reason
    print >>sys.stderr, "Failed to open %s: %s" % (url, message)
  return []


def _WebkitRevisionInDeps():
  """Returns the WebKit revision specified in DEPS file.

  Returns:
      Revision number as int. -1 in case of error.
  """
  for line in _OpenUrl(_DEPS_FILE_URL):
    match = _WEBKIT_REVISION_IN_DEPS_RE.search(line)
    if match:
      return int(match.group(1))
  return -1


class _BuildResult(object):
  """Build result for a builder.

  Holds builder name, the last passing revision, the last run revision, and
  a list of names of failing tests. Revision nubmer 0 is used to represent
  that the revision doesn't exist.
  """
  def __init__(self, builder, last_passing_revision, last_run_revision,
               failing_tests):
    """Constructs build results."""
    self.builder = builder
    self.last_passing_revision = last_passing_revision
    self.last_run_revision = last_run_revision
    self.failing_tests = failing_tests


def _BuilderUrlFor(builder, max_builds):
  """Constructs the URL for a builder to retrieve the last results."""
  url = ("http://build.chromium.org/p/chromium.webkit/json/builders/%s/builds" %
         urllib2.quote(builder))
  if max_builds == -1:
    return url + "/_all?as_text=1"
  return (url + "?as_text=1&" +
          '&'.join(["select=%d" % -i for i in range(1, 1 + max_builds)]))


def _ExtractFailingTests(build):
  """Extracts failing test names from a build result entry JSON object."""
  failing_tests = []
  for step in build["steps"]:
    if step["name"] == _WEBKIT_TESTS:
      for text in step["text"]:
        prefix = text.find(_TEST_PREFIX)
        suffix = text.find(_TEST_SUFFIX)
        if prefix != -1 and suffix != -1:
          failing_tests += sorted(
              text[prefix + len(_TEST_PREFIX): suffix].split(","))
    elif "results" in step:
      # Existence of "results" entry seems to mean failure.
      failing_tests.append(" ".join(step["text"]))
  return failing_tests


def _RetrieveBuildResult(builder, max_builds, oldest_revision_to_check):
  """Retrieves build results for a builder.

  Checks the last passing revision, the last run revision, and failing tests
  for the last builds of a builder.

  Args:
      builder: Builder name.
      max_builds: Maximum number of builds to check.
      oldest_revision_to_check: Oldest WebKit revision to check.

  Returns:
      _BuildResult instance.
  """
  last_run_revision = 0
  failing_tests = []
  succeeded = False
  builds_json = _OpenUrl(_BuilderUrlFor(builder, max_builds))
  if not builds_json:
    return _BuildResult(builder, 0, 0, failing_tests)
  builds = [(int(value["number"]), value) for unused_key, value
            in json.loads(''.join(builds_json)).items()
            if value.has_key("number")]
  builds.sort()
  builds.reverse()
  for unused_key, build in builds:
    if not build.has_key("text"):
      continue
    if len(build["text"]) < 2:
      continue
    if not build.has_key("sourceStamp"):
      continue
    if build["text"][1] == "successful":
      succeeded = True
    elif not failing_tests:
      failing_tests = _ExtractFailingTests(build)
    revision = 0
    if build["sourceStamp"]["branch"] == "trunk":
      revision = int(build["sourceStamp"]["changes"][-1]["revision"])
    if revision and not last_run_revision:
      last_run_revision = revision
    if revision and revision < oldest_revision_to_check:
      break
    if not succeeded or not revision:
      continue
    return _BuildResult(builder, revision, last_run_revision, failing_tests)
  return _BuildResult(builder, 0, last_run_revision, failing_tests)


def _PrintPassingRevisions(results, unused_verbose):
  """Prints passing revisions and the range of such revisions.

  Args:
      results: A list of build results.
  """
  print "**** Passing revisions *****"
  min_passing_revision = sys.maxint
  max_passing_revision = 0
  for result in results:
    if result.last_passing_revision:
      min_passing_revision = min(min_passing_revision,
                                 result.last_passing_revision)
      max_passing_revision = max(max_passing_revision,
                                 result.last_passing_revision)
      print 'The last passing run was at r%d on "%s"' % (
          result.last_passing_revision, result.builder)
    else:
      print 'No passing runs on "%s"' % result.builder
  if max_passing_revision:
    print "Passing revision range: r%d - r%d" % (
          min_passing_revision, max_passing_revision)


def _PrintFailingRevisions(results, verbose):
  """Prints failing revisions and the failing tests.

  Args:
      results: A list of build results.
  """
  failing_test_to_builders = {}
  print "**** Failing revisions *****"
  for result in results:
    if result.last_run_revision and result.failing_tests:
      print ('The last run was at r%d on "%s" and the following %d tests'
             ' failed' % (result.last_run_revision, result.builder,
                          len(result.failing_tests)))
      for test in result.failing_tests:
        print "  " + test
        failing_test_to_builders.setdefault(test, set()).add(result.builder)
  if verbose:
    _PrintFailingTestsForBuilderSubsets(failing_test_to_builders)


class _FailingTestsForBuilderSubset(object):
  def __init__(self, subset_size):
    self._subset_size = subset_size
    self._tests = []

  def SubsetSize(self):
    return self._subset_size

  def Tests(self):
    return self._tests


def _PrintFailingTestsForBuilderSubsets(failing_test_to_builders):
  """Prints failing test for builder subsets.

  Prints failing tests for each subset of builders, in descending order of the
  set size.
  """
  print "**** Failing tests ****"
  builders_to_tests = {}
  for test in failing_test_to_builders:
    builders = sorted(failing_test_to_builders[test])
    subset_name = ", ".join(builders)
    tests = builders_to_tests.setdefault(
        subset_name, _FailingTestsForBuilderSubset(len(builders))).Tests()
    tests.append(test)
  # Sort subsets in descending order of size and then name.
  builder_subsets = [(builders_to_tests[subset_name].SubsetSize(), subset_name)
                     for subset_name in builders_to_tests]
  for subset_size, subset_name in reversed(sorted(builder_subsets)):
    print "** Tests failing for %d builders: %s **" % (subset_size,
                                                       subset_name)
    for test in sorted(builders_to_tests[subset_name].Tests()):
      print test


def _ParseOptions():
  """Parses command-line options."""
  parser = optparse.OptionParser(usage="%prog [options] [builders]")
  parser.add_option("-m", "--max_builds", type="int",
                    default=-1,
                    help="Maximum number of builds to check for each builder."
                         " Defaults to all builds for which record is"
                         " available. Checking is ended either when the maximum"
                         " number is reached, the remaining builds are older"
                         " than the DEPS WebKit revision, or a passing"
                         " revision is found.")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    dest="verbose")
  return parser.parse_args()


def _Main():
  """The main function."""
  options, builders = _ParseOptions()
  if not builders:
    builders = _DEFAULT_BUILDERS
  oldest_revision_to_check = _WebkitRevisionInDeps()
  if options.max_builds == -1 and oldest_revision_to_check == -1:
    options.max_builds = _DEFAULT_MAX_BUILDS
  if options.max_builds != -1:
    print "Maxium number of builds to check: %d" % options.max_builds
  if oldest_revision_to_check != -1:
    print "Oldest revision to check: %d" % oldest_revision_to_check
  sys.stdout.flush()
  results = []
  for builder in builders:
    print '"%s"' % builder
    sys.stdout.flush()
    results.append(_RetrieveBuildResult(
        builder, options.max_builds, oldest_revision_to_check))
  _PrintFailingRevisions(results, options.verbose)
  _PrintPassingRevisions(results, options.verbose)


if __name__ == "__main__":
  _Main()
