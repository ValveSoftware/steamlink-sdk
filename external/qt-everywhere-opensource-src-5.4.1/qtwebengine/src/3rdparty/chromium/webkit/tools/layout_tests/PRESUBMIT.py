# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""test_expectations.txt presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

TEST_EXPECTATIONS_FILENAMES = ['test_expectations.txt', 'TestExpectations']

def LintTestFiles(input_api, output_api):
  current_dir = str(input_api.PresubmitLocalPath())
  # Set 'webkit/tools/layout_tests' in include path.
  python_paths = [
      current_dir,
      input_api.os_path.join(current_dir, '..', '..', '..', 'tools', 'python')
  ]
  env = input_api.environ.copy()
  if env.get('PYTHONPATH'):
    python_paths.append(env['PYTHONPATH'])
  env['PYTHONPATH'] = input_api.os_path.pathsep.join(python_paths)
  args = [
      input_api.python_executable,
      input_api.os_path.join(current_dir, 'run_webkit_tests.py'),
      '--lint-test-files'
  ]
  subproc = input_api.subprocess.Popen(
      args,
      cwd=current_dir,
      env=env,
      stdin=input_api.subprocess.PIPE,
      stdout=input_api.subprocess.PIPE,
      stderr=input_api.subprocess.STDOUT)
  stdout_data = subproc.communicate()[0]
  # TODO(ukai): consolidate run_webkit_tests --lint-test-files reports.
  is_error = lambda line: (input_api.re.match('^Line:', line) or
                           input_api.re.search('ERROR Line:', line))
  error = filter(is_error, stdout_data.splitlines())
  if error:
    return [output_api.PresubmitError('Lint error\n%s' % '\n'.join(error),
                                      long_text=stdout_data)]
  return []

def LintTestExpectations(input_api, output_api):
  for path in input_api.LocalPaths():
    if input_api.os_path.basename(path) in TEST_EXPECTATIONS_FILENAMES:
      return LintTestFiles(input_api, output_api)
  return []


def CheckChangeOnUpload(input_api, output_api):
  return LintTestExpectations(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return LintTestExpectations(input_api, output_api)
