#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main function to run the layout test analyzer.

The purpose of this script is to run the layout test analyzer for various
teams based on the run configuration file in CSV format. The CSV file is based
on https://sites.google.com/a/chromium.org/dev/developers/testing/
webkit-layout-tests/layout-test-stats-1.
"""

import optparse
import os
import shutil
from subprocess import Popen

# TODO(shadi): Re-examine the need of external files. Inline data instead?
DEFAULT_RUN_CONFIG = {
    # test_group_name: ('test_files.csv', 'report_email_address')
    'media': ('testname/media.csv', 'layout-test-analyzer-result@google.com')
}

# Predefined result/graph directory.
DEFAULT_RESULT_DIR = 'result'
DEFAULT_GRAPH_DIR = 'graph'


def ParseOption():
  """Parse command-line options using OptionParser.

  Returns:
      an object containing all command-line option information.
  """
  option_parser = optparse.OptionParser()
  option_parser.add_option('-d', '--result-directory-location',
                           dest='result_directory_location',
                           help=('Name of result directory location '
                                 '(default to %default)'),
                           default=DEFAULT_RESULT_DIR)
  option_parser.add_option('-p', '--graph-directory-location',
                           dest='graph_directory_location',
                           help=('Name of graph directory location '
                                 '(default to %default)'),
                           default=DEFAULT_GRAPH_DIR)
  option_parser.add_option('-e', '--email-only-change-mode',
                           dest='email_only_change_mode',
                           help=('With this mode, email is sent out '
                                 'only when there is a change in the '
                                 'analyzer result compared to the previous '
                                 'result (off by default)'),
                           action='store_true', default=False)
  option_parser.add_option('-z', '--issue-detail-mode',
                           dest='issue_detail_mode',
                           help=('With this mode, email includes issue details'
                                 ' including links to the flakiness dashboard'
                                 ' (off by default)'),
                           action='store_true', default=False)
  return option_parser.parse_args()[0]


def GenerateDashboardHTMLFile(file_name, test_group_list):
  """Generate dashboard HTML file.

  Currently, it is simple table that shows all the analyzer results.

  Args:
    file_name: the file name of the dashboard.
    test_group_list: a list of test group names such as 'media' or 'composite'.
  """
  file_object = open(file_name, 'wb')
  legend_txt = """
<style type="text/css">
th {
  width: 30px; overflow: hidden;
}
tr.d0 td {
  background-color: #CC9999; color: black;
  text-align: right;
  width: 30px; overflow: hidden;
}
tr.d1 td {
  background-color: #9999CC; color: black;
  text-align: right;
  width: 30px; overflow: hidden;
}
</style>
<h2>Chromium Layout Test Analyzer Result</h2>
Legend:
<ul>
<li>#Tests: the number of tests for the given test group
<li>#Skipped Tests: the number of tests that are skipped in the
<a href='http://svn.webkit.org/repository/webkit/trunk/LayoutTests/platform/\
chromium/test_expectations.txt'>test expectaion file</a> (e.g., BUGWK60877
SKIP : loader/navigation-while-deferring-loads.html = FAIL)
<li>#Non-Skipped Failing Tests: the number of tests that appeared in the
test expectation file and were not skipped.
<li>Failing rate: #NonSkippedFailing / (#Tests - #Skipped)
<li>Passing rate: 100 - (Failing rate)
</ul>
  """
  file_object.write(legend_txt)
  file_object.write('<table border="1">')
  file_object.write('<tr><th>Base Directory</th>')
  file_object.write('<th>Trend Graph</th>')
  file_object.write('<th>#Tests</th>')
  file_object.write('<th>#Skipped Tests</th>')
  file_object.write('<th>#Non-Skipped Failing Tests</th>')
  file_object.write('<th>Failing Rate</th>')
  file_object.write('<th>Passing Rate</th>')
  file_object.write('<th>Last Revision Number</th>')
  file_object.write('<th>Last Revision Date</th>')
  file_object.write('<th>Owner Email</th>')
  file_object.write('<th>Bug Information</th></tr>\n')
  test_group_list.sort()
  for i, test_group in enumerate(test_group_list):
    file_object.write('<tr class="d' + str(i % 2) + '">\n')
    file_object.write('<td>' + test_group + '</td>\n')
    file_object.write('</tr>\n')
  file_object.write('</table>')
  file_object.close()


# TODO(shadi): Use only one file with main()! Remove this file in favor of
# layouttest_analyzer.py main().
def main():
  """A main function for the analyzer runner."""
  options = ParseOption()
  run_config_map = DEFAULT_RUN_CONFIG
  test_group_list = run_config_map.keys()
  dashboard_file_location = os.path.join(options.graph_directory_location,
                                         'index.html')
  if not os.path.exists(dashboard_file_location):
    GenerateDashboardHTMLFile(dashboard_file_location, test_group_list)
  for test_group in test_group_list:
    # Prepare the result if it does not exist.
    # The directory name should be changed to avoid collision
    # with the file separator.
    test_group_name_for_data = test_group.replace('/', '_')
    result_dir = os.path.join(options.result_directory_location,
                              test_group_name_for_data)
    if not os.path.exists(result_dir):
      os.mkdir(result_dir)
    graph_file = os.path.join(options.graph_directory_location,
                              test_group_name_for_data + '.html')
    if not os.path.exists(graph_file):
      # Copy the template file.
      shutil.copy(os.path.join('graph', 'graph.html'),
                  graph_file)
      os.chmod(graph_file, 0744)
    cmd = ('python layouttest_analyzer.py -x %s -d %s -t %s'
           ' -q %s ') % (
               test_group, result_dir, graph_file, dashboard_file_location)
    if run_config_map[test_group][0]:
      cmd += '-n ' + run_config_map[test_group][0] + ' '
    if run_config_map[test_group][1]:
      cmd += '-r ' + run_config_map[test_group][1] + ' '
    if options.email_only_change_mode:
      cmd += ' -c '
    if options.issue_detail_mode:
      cmd += ' -z '
    print 'Running ' + cmd
    proc = Popen(cmd, shell=True)
    proc.communicate()


if '__main__' == __name__:
  main()
