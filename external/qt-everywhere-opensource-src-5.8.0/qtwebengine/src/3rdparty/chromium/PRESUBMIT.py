# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


_EXCLUDED_PATHS = (
    r"^breakpad[\\\/].*",
    r"^native_client_sdk[\\\/]src[\\\/]build_tools[\\\/]make_rules.py",
    r"^native_client_sdk[\\\/]src[\\\/]build_tools[\\\/]make_simple.py",
    r"^native_client_sdk[\\\/]src[\\\/]tools[\\\/].*.mk",
    r"^net[\\\/]tools[\\\/]spdyshark[\\\/].*",
    r"^skia[\\\/].*",
    r"^third_party[\\\/]WebKit[\\\/].*",
    r"^v8[\\\/].*",
    r".*MakeFile$",
    r".+_autogen\.h$",
    r".+[\\\/]pnacl_shim\.c$",
    r"^gpu[\\\/]config[\\\/].*_list_json\.cc$",
    r"^chrome[\\\/]browser[\\\/]resources[\\\/]pdf[\\\/]index.js",
)


# The NetscapePlugIn library is excluded from pan-project as it will soon
# be deleted together with the rest of the NPAPI and it's not worthwhile to
# update the coding style until then.
_TESTRUNNER_PATHS = (
    r"^content[\\\/]shell[\\\/]tools[\\\/]plugin[\\\/].*",
)


# Fragment of a regular expression that matches C++ and Objective-C++
# implementation files.
_IMPLEMENTATION_EXTENSIONS = r'\.(cc|cpp|cxx|mm)$'


# Regular expression that matches code only used for test binaries
# (best effort).
_TEST_CODE_EXCLUDED_PATHS = (
    r'.*[\\\/](fake_|test_|mock_).+%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.+_test_(base|support|util)%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.+_(api|browser|kif|perf|pixel|unit|ui)?test(_[a-z]+)?%s' %
        _IMPLEMENTATION_EXTENSIONS,
    r'.+profile_sync_service_harness%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.*[\\\/](test|tool(s)?)[\\\/].*',
    # content_shell is used for running layout tests.
    r'content[\\\/]shell[\\\/].*',
    # At request of folks maintaining this folder.
    r'chrome[\\\/]browser[\\\/]automation[\\\/].*',
    # Non-production example code.
    r'mojo[\\\/]examples[\\\/].*',
    # Launcher for running iOS tests on the simulator.
    r'testing[\\\/]iossim[\\\/]iossim\.mm$',
)


_TEST_ONLY_WARNING = (
    'You might be calling functions intended only for testing from\n'
    'production code.  It is OK to ignore this warning if you know what\n'
    'you are doing, as the heuristics used to detect the situation are\n'
    'not perfect.  The commit queue will not block on this warning.')


_INCLUDE_ORDER_WARNING = (
    'Your #include order seems to be broken. Remember to use the right '
    'collation (LC_COLLATE=C) and check\nhttps://google.github.io/styleguide/'
    'cppguide.html#Names_and_Order_of_Includes')


_BANNED_OBJC_FUNCTIONS = (
    (
      'addTrackingRect:',
      (
       'The use of -[NSView addTrackingRect:owner:userData:assumeInside:] is'
       'prohibited. Please use CrTrackingArea instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      False,
    ),
    (
      r'/NSTrackingArea\W',
      (
       'The use of NSTrackingAreas is prohibited. Please use CrTrackingArea',
       'instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      False,
    ),
    (
      'convertPointFromBase:',
      (
       'The use of -[NSView convertPointFromBase:] is almost certainly wrong.',
       'Please use |convertPoint:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertPointToBase:',
      (
       'The use of -[NSView convertPointToBase:] is almost certainly wrong.',
       'Please use |convertPoint:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertRectFromBase:',
      (
       'The use of -[NSView convertRectFromBase:] is almost certainly wrong.',
       'Please use |convertRect:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertRectToBase:',
      (
       'The use of -[NSView convertRectToBase:] is almost certainly wrong.',
       'Please use |convertRect:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertSizeFromBase:',
      (
       'The use of -[NSView convertSizeFromBase:] is almost certainly wrong.',
       'Please use |convertSize:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertSizeToBase:',
      (
       'The use of -[NSView convertSizeToBase:] is almost certainly wrong.',
       'Please use |convertSize:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
)


_BANNED_CPP_FUNCTIONS = (
    # Make sure that gtest's FRIEND_TEST() macro is not used; the
    # FRIEND_TEST_ALL_PREFIXES() macro from base/gtest_prod_util.h should be
    # used instead since that allows for FLAKY_ and DISABLED_ prefixes.
    (
      'FRIEND_TEST(',
      (
       'Chromium code should not use gtest\'s FRIEND_TEST() macro. Include',
       'base/gtest_prod_util.h and use FRIEND_TEST_ALL_PREFIXES() instead.',
      ),
      False,
      (),
    ),
    (
      'ScopedAllowIO',
      (
       'New code should not use ScopedAllowIO. Post a task to the blocking',
       'pool or the FILE thread instead.',
      ),
      True,
      (
        r"^base[\\\/]process[\\\/]process_linux\.cc$",
        r"^base[\\\/]process[\\\/]process_metrics_linux\.cc$",
        r"^blimp[\\\/]engine[\\\/]app[\\\/]blimp_browser_main_parts\.cc$",
        r"^chrome[\\\/]browser[\\\/]chromeos[\\\/]boot_times_recorder\.cc$",
        r"^chrome[\\\/]browser[\\\/]lifetime[\\\/]application_lifetime\.cc$",
        r"^chrome[\\\/]browser[\\\/]chromeos[\\\/]"
            "customization_document_browsertest\.cc$",
        r"^components[\\\/]crash[\\\/]app[\\\/]breakpad_mac\.mm$",
        r"^content[\\\/]shell[\\\/]browser[\\\/]layout_test[\\\/]" +
            r"test_info_extractor\.cc$",
        r"^content[\\\/]shell[\\\/]browser[\\\/]shell_browser_main\.cc$",
        r"^content[\\\/]shell[\\\/]browser[\\\/]shell_message_filter\.cc$",
        r"^mojo[\\\/]edk[\\\/]embedder[\\\/]" +
            r"simple_platform_shared_buffer_posix\.cc$",
        r"^net[\\\/]disk_cache[\\\/]cache_util\.cc$",
        r"^net[\\\/]url_request[\\\/]test_url_fetcher_factory\.cc$",
        r"^remoting[\\\/]host[\\\/]security_key[\\\/]"
            "gnubby_auth_handler_linux\.cc$",
        r"^ui[\\\/]ozone[\\\/]platform[\\\/]drm[\\\/]host[\\\/]"
            "drm_display_host_manager\.cc$",
        r"^ui[\\\/]base[\\\/]material_design[\\\/]"
            "material_design_controller\.cc$",
      ),
    ),
    (
      'setMatrixClip',
      (
        'Overriding setMatrixClip() is prohibited; ',
        'the base function is deprecated. ',
      ),
      True,
      (),
    ),
    (
      'SkRefPtr',
      (
        'The use of SkRefPtr is prohibited. ',
        'Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      'SkAutoRef',
      (
        'The indirect use of SkRefPtr via SkAutoRef is prohibited. ',
        'Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      'SkAutoTUnref',
      (
        'The use of SkAutoTUnref is dangerous because it implicitly ',
        'converts to a raw pointer. Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      'SkAutoUnref',
      (
        'The indirect use of SkAutoTUnref through SkAutoUnref is dangerous ',
        'because it implicitly converts to a raw pointer. ',
        'Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      r'/HANDLE_EINTR\(.*close',
      (
       'HANDLE_EINTR(close) is invalid. If close fails with EINTR, the file',
       'descriptor will be closed, and it is incorrect to retry the close.',
       'Either call close directly and ignore its return value, or wrap close',
       'in IGNORE_EINTR to use its return value. See http://crbug.com/269623'
      ),
      True,
      (),
    ),
    (
      r'/IGNORE_EINTR\((?!.*close)',
      (
       'IGNORE_EINTR is only valid when wrapping close. To wrap other system',
       'calls, use HANDLE_EINTR. See http://crbug.com/269623',
      ),
      True,
      (
        # Files that #define IGNORE_EINTR.
        r'^base[\\\/]posix[\\\/]eintr_wrapper\.h$',
        r'^ppapi[\\\/]tests[\\\/]test_broker\.cc$',
      ),
    ),
    (
      r'/v8::Extension\(',
      (
        'Do not introduce new v8::Extensions into the code base, use',
        'gin::Wrappable instead. See http://crbug.com/334679',
      ),
      True,
      (
        r'extensions[\\\/]renderer[\\\/]safe_builtins\.*',
      ),
    ),
    (
      '\<MessageLoopProxy\>',
      (
        'MessageLoopProxy is deprecated. ',
        'Please use SingleThreadTaskRunner or ThreadTaskRunnerHandle instead.'
      ),
      True,
      (
        # Internal message_loop related code may still use it.
        r'^base[\\\/]message_loop[\\\/].*',
      ),
    ),
    (
      '#pragma comment(lib,',
      (
        'Specify libraries to link with in build files and not in the source.',
      ),
      True,
      (),
    ),
)


_IPC_ENUM_TRAITS_DEPRECATED = (
    'You are using IPC_ENUM_TRAITS() in your code. It has been deprecated.\n'
    'See http://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc')


_VALID_OS_MACROS = (
    # Please keep sorted.
    'OS_ANDROID',
    'OS_BSD',
    'OS_CAT',       # For testing.
    'OS_CHROMEOS',
    'OS_FREEBSD',
    'OS_IOS',
    'OS_LINUX',
    'OS_MACOSX',
    'OS_NACL',
    'OS_NACL_NONSFI',
    'OS_NACL_SFI',
    'OS_OPENBSD',
    'OS_POSIX',
    'OS_QNX',
    'OS_SOLARIS',
    'OS_WIN',
)


_ANDROID_SPECIFIC_PYDEPS_FILES = [
    'build/android/test_runner.pydeps',
    'net/tools/testserver/testserver.pydeps',
]


_GENERIC_PYDEPS_FILES = [
    'build/secondary/tools/swarming_client/isolate.pydeps',
]


_ALL_PYDEPS_FILES = _ANDROID_SPECIFIC_PYDEPS_FILES + _GENERIC_PYDEPS_FILES


def _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api):
  """Attempts to prevent use of functions intended only for testing in
  non-testing code. For now this is just a best-effort implementation
  that ignores header files and may have some false positives. A
  better implementation would probably need a proper C++ parser.
  """
  # We only scan .cc files and the like, as the declaration of
  # for-testing functions in header files are hard to distinguish from
  # calls to such functions without a proper C++ parser.
  file_inclusion_pattern = r'.+%s' % _IMPLEMENTATION_EXTENSIONS

  base_function_pattern = r'[ :]test::[^\s]+|ForTest(s|ing)?|for_test(s|ing)?'
  inclusion_pattern = input_api.re.compile(r'(%s)\s*\(' % base_function_pattern)
  comment_pattern = input_api.re.compile(r'//.*(%s)' % base_function_pattern)
  exclusion_pattern = input_api.re.compile(
    r'::[A-Za-z0-9_]+(%s)|(%s)[^;]+\{' % (
      base_function_pattern, base_function_pattern))

  def FilterFile(affected_file):
    black_list = (_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST)
    return input_api.FilterSourceFile(
      affected_file,
      white_list=(file_inclusion_pattern, ),
      black_list=black_list)

  problems = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    local_path = f.LocalPath()
    for line_number, line in f.ChangedContents():
      if (inclusion_pattern.search(line) and
          not comment_pattern.search(line) and
          not exclusion_pattern.search(line)):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(_TEST_ONLY_WARNING, problems)]
  else:
    return []


def _CheckNoIOStreamInHeaders(input_api, output_api):
  """Checks to make sure no .h files include <iostream>."""
  files = []
  pattern = input_api.re.compile(r'^#include\s*<iostream>',
                                 input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not f.LocalPath().endswith('.h'):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if len(files):
    return [output_api.PresubmitError(
        'Do not #include <iostream> in header files, since it inserts static '
        'initialization into every file including the header. Instead, '
        '#include <ostream>. See http://crbug.com/94794',
        files) ]
  return []


def _CheckNoUNIT_TESTInSourceFiles(input_api, output_api):
  """Checks to make sure no source files use UNIT_TEST."""
  problems = []
  for f in input_api.AffectedFiles():
    if (not f.LocalPath().endswith(('.cc', '.mm'))):
      continue

    for line_num, line in f.ChangedContents():
      if 'UNIT_TEST ' in line or line.endswith('UNIT_TEST'):
        problems.append('    %s:%d' % (f.LocalPath(), line_num))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('UNIT_TEST is only for headers.\n' +
      '\n'.join(problems))]


def _CheckDCHECK_IS_ONHasBraces(input_api, output_api):
  """Checks to make sure DCHECK_IS_ON() does not skip the braces."""
  errors = []
  pattern = input_api.re.compile(r'DCHECK_IS_ON(?!\(\))',
                                 input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (not f.LocalPath().endswith(('.cc', '.mm', '.h'))):
      continue
    for lnum, line in f.ChangedContents():
      if input_api.re.search(pattern, line):
        errors.append(output_api.PresubmitError(
          ('%s:%d: Use of DCHECK_IS_ON() must be written as "#if ' +
           'DCHECK_IS_ON()", not forgetting the braces.')
          % (f.LocalPath(), lnum)))
  return errors


def _FindHistogramNameInLine(histogram_name, line):
  """Tries to find a histogram name or prefix in a line."""
  if not "affected-histogram" in line:
    return histogram_name in line
  # A histogram_suffixes tag type has an affected-histogram name as a prefix of
  # the histogram_name.
  if not '"' in line:
    return False
  histogram_prefix = line.split('\"')[1]
  return histogram_prefix in histogram_name


def _CheckUmaHistogramChanges(input_api, output_api):
  """Check that UMA histogram names in touched lines can still be found in other
  lines of the patch or in histograms.xml. Note that this check would not catch
  the reverse: changes in histograms.xml not matched in the code itself."""
  touched_histograms = []
  histograms_xml_modifications = []
  pattern = input_api.re.compile('UMA_HISTOGRAM.*\("(.*)"')
  for f in input_api.AffectedFiles():
    # If histograms.xml itself is modified, keep the modified lines for later.
    if f.LocalPath().endswith(('histograms.xml')):
      histograms_xml_modifications = f.ChangedContents()
      continue
    if not f.LocalPath().endswith(('cc', 'mm', 'cpp')):
      continue
    for line_num, line in f.ChangedContents():
      found = pattern.search(line)
      if found:
        touched_histograms.append([found.group(1), f, line_num])

  # Search for the touched histogram names in the local modifications to
  # histograms.xml, and, if not found, on the base histograms.xml file.
  unmatched_histograms = []
  for histogram_info in touched_histograms:
    histogram_name_found = False
    for line_num, line in histograms_xml_modifications:
      histogram_name_found = _FindHistogramNameInLine(histogram_info[0], line)
      if histogram_name_found:
        break
    if not histogram_name_found:
      unmatched_histograms.append(histogram_info)

  histograms_xml_path = 'tools/metrics/histograms/histograms.xml'
  problems = []
  if unmatched_histograms:
    with open(histograms_xml_path) as histograms_xml:
      for histogram_name, f, line_num in unmatched_histograms:
        histograms_xml.seek(0)
        histogram_name_found = False
        for line in histograms_xml:
          histogram_name_found = _FindHistogramNameInLine(histogram_name, line)
          if histogram_name_found:
            break
        if not histogram_name_found:
          problems.append(' [%s:%d] %s' %
                          (f.LocalPath(), line_num, histogram_name))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('Some UMA_HISTOGRAM lines have '
    'been modified and the associated histogram name has no match in either '
    '%s or the modifications of it:' % (histograms_xml_path),  problems)]


def _CheckFlakyTestUsage(input_api, output_api):
  """Check that FlakyTest annotation is our own instead of the android one"""
  pattern = input_api.re.compile(r'import android.test.FlakyTest;')
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if f.LocalPath().endswith('Test.java'):
      if pattern.search(input_api.ReadFile(f)):
        files.append(f)
  if len(files):
    return [output_api.PresubmitError(
      'Use org.chromium.base.test.util.FlakyTest instead of '
      'android.test.FlakyTest',
      files)]
  return []


def _CheckNoNewWStrings(input_api, output_api):
  """Checks to make sure we don't introduce use of wstrings."""
  problems = []
  for f in input_api.AffectedFiles():
    if (not f.LocalPath().endswith(('.cc', '.h')) or
        f.LocalPath().endswith(('test.cc', '_win.cc', '_win.h')) or
        '/win/' in f.LocalPath()):
      continue

    allowWString = False
    for line_num, line in f.ChangedContents():
      if 'presubmit: allow wstring' in line:
        allowWString = True
      elif not allowWString and 'wstring' in line:
        problems.append('    %s:%d' % (f.LocalPath(), line_num))
        allowWString = False
      else:
        allowWString = False

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('New code should not use wstrings.'
      '  If you are calling a cross-platform API that accepts a wstring, '
      'fix the API.\n' +
      '\n'.join(problems))]


def _CheckNoDEPSGIT(input_api, output_api):
  """Make sure .DEPS.git is never modified manually."""
  if any(f.LocalPath().endswith('.DEPS.git') for f in
      input_api.AffectedFiles()):
    return [output_api.PresubmitError(
      'Never commit changes to .DEPS.git. This file is maintained by an\n'
      'automated system based on what\'s in DEPS and your changes will be\n'
      'overwritten.\n'
      'See https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code#Rolling_DEPS\n'
      'for more information')]
  return []


def _CheckValidHostsInDEPS(input_api, output_api):
  """Checks that DEPS file deps are from allowed_hosts."""
  # Run only if DEPS file has been modified to annoy fewer bystanders.
  if all(f.LocalPath() != 'DEPS' for f in input_api.AffectedFiles()):
    return []
  # Outsource work to gclient verify
  try:
    input_api.subprocess.check_output(['gclient', 'verify'])
    return []
  except input_api.subprocess.CalledProcessError, error:
    return [output_api.PresubmitError(
        'DEPS file must have only git dependencies.',
        long_text=error.output)]


def _CheckNoBannedFunctions(input_api, output_api):
  """Make sure that banned functions are not used."""
  warnings = []
  errors = []

  def IsBlacklisted(affected_file, blacklist):
    local_path = affected_file.LocalPath()
    for item in blacklist:
      if input_api.re.match(item, local_path):
        return True
    return False

  def CheckForMatch(affected_file, line_num, line, func_name, message, error):
    matched = False
    if func_name[0:1] == '/':
      regex = func_name[1:]
      if input_api.re.search(regex, line):
        matched = True
    elif func_name in line:
      matched = True
    if matched:
      problems = warnings
      if error:
        problems = errors
      problems.append('    %s:%d:' % (affected_file.LocalPath(), line_num))
      for message_line in message:
        problems.append('      %s' % message_line)

  file_filter = lambda f: f.LocalPath().endswith(('.mm', '.m', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, error in _BANNED_OBJC_FUNCTIONS:
        CheckForMatch(f, line_num, line, func_name, message, error)

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, error, excluded_paths in _BANNED_CPP_FUNCTIONS:
        if IsBlacklisted(f, excluded_paths):
          continue
        CheckForMatch(f, line_num, line, func_name, message, error)

  result = []
  if (warnings):
    result.append(output_api.PresubmitPromptWarning(
        'Banned functions were used.\n' + '\n'.join(warnings)))
  if (errors):
    result.append(output_api.PresubmitError(
        'Banned functions were used.\n' + '\n'.join(errors)))
  return result


def _CheckNoPragmaOnce(input_api, output_api):
  """Make sure that banned functions are not used."""
  files = []
  pattern = input_api.re.compile(r'^#pragma\s+once',
                                 input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not f.LocalPath().endswith('.h'):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if files:
    return [output_api.PresubmitError(
        'Do not use #pragma once in header files.\n'
        'See http://www.chromium.org/developers/coding-style#TOC-File-headers',
        files)]
  return []


def _CheckNoTrinaryTrueFalse(input_api, output_api):
  """Checks to make sure we don't introduce use of foo ? true : false."""
  problems = []
  pattern = input_api.re.compile(r'\?\s*(true|false)\s*:\s*(true|false)')
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith(('.cc', '.h', '.inl', '.m', '.mm')):
      continue

    for line_num, line in f.ChangedContents():
      if pattern.match(line):
        problems.append('    %s:%d' % (f.LocalPath(), line_num))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning(
      'Please consider avoiding the "? true : false" pattern if possible.\n' +
      '\n'.join(problems))]


def _CheckUnwantedDependencies(input_api, output_api):
  """Runs checkdeps on #include statements added in this
  change. Breaking - rules is an error, breaking ! rules is a
  warning.
  """
  import sys
  # We need to wait until we have an input_api object and use this
  # roundabout construct to import checkdeps because this file is
  # eval-ed and thus doesn't have __file__.
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'buildtools', 'checkdeps')]
    import checkdeps
    from cpp_checker import CppChecker
    from rules import Rule
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  added_includes = []
  for f in input_api.AffectedFiles():
    if not CppChecker.IsCppFile(f.LocalPath()):
      continue

    changed_lines = [line for line_num, line in f.ChangedContents()]
    added_includes.append([f.LocalPath(), changed_lines])

  deps_checker = checkdeps.DepsChecker(input_api.PresubmitLocalPath())

  error_descriptions = []
  warning_descriptions = []
  for path, rule_type, rule_description in deps_checker.CheckAddedCppIncludes(
      added_includes):
    description_with_path = '%s\n    %s' % (path, rule_description)
    if rule_type == Rule.DISALLOW:
      error_descriptions.append(description_with_path)
    else:
      warning_descriptions.append(description_with_path)

  results = []
  if error_descriptions:
    results.append(output_api.PresubmitError(
        'You added one or more #includes that violate checkdeps rules.',
        error_descriptions))
  if warning_descriptions:
    results.append(output_api.PresubmitPromptOrNotify(
        'You added one or more #includes of files that are temporarily\n'
        'allowed but being removed. Can you avoid introducing the\n'
        '#include? See relevant DEPS file(s) for details and contacts.',
        warning_descriptions))
  return results


def _CheckFilePermissions(input_api, output_api):
  """Check that all files have their permissions properly set."""
  if input_api.platform == 'win32':
    return []
  checkperms_tool = input_api.os_path.join(
      input_api.PresubmitLocalPath(),
      'tools', 'checkperms', 'checkperms.py')
  args = [input_api.python_executable, checkperms_tool,
          '--root', input_api.change.RepositoryRoot()]
  for f in input_api.AffectedFiles():
    args += ['--file', f.LocalPath()]
  try:
    input_api.subprocess.check_output(args)
    return []
  except input_api.subprocess.CalledProcessError as error:
    return [output_api.PresubmitError(
        'checkperms.py failed:',
        long_text=error.output)]


def _CheckNoAuraWindowPropertyHInHeaders(input_api, output_api):
  """Makes sure we don't include ui/aura/window_property.h
  in header files.
  """
  pattern = input_api.re.compile(r'^#include\s*"ui/aura/window_property.h"')
  errors = []
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith('.h'):
      continue
    for line_num, line in f.ChangedContents():
      if pattern.match(line):
        errors.append('    %s:%d' % (f.LocalPath(), line_num))

  results = []
  if errors:
    results.append(output_api.PresubmitError(
      'Header files should not include ui/aura/window_property.h', errors))
  return results


def _CheckIncludeOrderForScope(scope, input_api, file_path, changed_linenums):
  """Checks that the lines in scope occur in the right order.

  1. C system files in alphabetical order
  2. C++ system files in alphabetical order
  3. Project's .h files
  """

  c_system_include_pattern = input_api.re.compile(r'\s*#include <.*\.h>')
  cpp_system_include_pattern = input_api.re.compile(r'\s*#include <.*>')
  custom_include_pattern = input_api.re.compile(r'\s*#include ".*')

  C_SYSTEM_INCLUDES, CPP_SYSTEM_INCLUDES, CUSTOM_INCLUDES = range(3)

  state = C_SYSTEM_INCLUDES

  previous_line = ''
  previous_line_num = 0
  problem_linenums = []
  out_of_order = " - line belongs before previous line"
  for line_num, line in scope:
    if c_system_include_pattern.match(line):
      if state != C_SYSTEM_INCLUDES:
        problem_linenums.append((line_num, previous_line_num,
            " - C system include file in wrong block"))
      elif previous_line and previous_line > line:
        problem_linenums.append((line_num, previous_line_num,
            out_of_order))
    elif cpp_system_include_pattern.match(line):
      if state == C_SYSTEM_INCLUDES:
        state = CPP_SYSTEM_INCLUDES
      elif state == CUSTOM_INCLUDES:
        problem_linenums.append((line_num, previous_line_num,
            " - c++ system include file in wrong block"))
      elif previous_line and previous_line > line:
        problem_linenums.append((line_num, previous_line_num, out_of_order))
    elif custom_include_pattern.match(line):
      if state != CUSTOM_INCLUDES:
        state = CUSTOM_INCLUDES
      elif previous_line and previous_line > line:
        problem_linenums.append((line_num, previous_line_num, out_of_order))
    else:
      problem_linenums.append((line_num, previous_line_num,
          "Unknown include type"))
    previous_line = line
    previous_line_num = line_num

  warnings = []
  for (line_num, previous_line_num, failure_type) in problem_linenums:
    if line_num in changed_linenums or previous_line_num in changed_linenums:
      warnings.append('    %s:%d:%s' % (file_path, line_num, failure_type))
  return warnings


def _CheckIncludeOrderInFile(input_api, f, changed_linenums):
  """Checks the #include order for the given file f."""

  system_include_pattern = input_api.re.compile(r'\s*#include \<.*')
  # Exclude the following includes from the check:
  # 1) #include <.../...>, e.g., <sys/...> includes often need to appear in a
  # specific order.
  # 2) <atlbase.h>, "build/build_config.h"
  excluded_include_pattern = input_api.re.compile(
      r'\s*#include (\<.*/.*|\<atlbase\.h\>|"build/build_config.h")')
  custom_include_pattern = input_api.re.compile(r'\s*#include "(?P<FILE>.*)"')
  # Match the final or penultimate token if it is xxxtest so we can ignore it
  # when considering the special first include.
  test_file_tag_pattern = input_api.re.compile(
    r'_[a-z]+test(?=(_[a-zA-Z0-9]+)?\.)')
  if_pattern = input_api.re.compile(
      r'\s*#\s*(if|elif|else|endif|define|undef).*')
  # Some files need specialized order of includes; exclude such files from this
  # check.
  uncheckable_includes_pattern = input_api.re.compile(
      r'\s*#include '
      '("ipc/.*macros\.h"|<windows\.h>|".*gl.*autogen.h")\s*')

  contents = f.NewContents()
  warnings = []
  line_num = 0

  # Handle the special first include. If the first include file is
  # some/path/file.h, the corresponding including file can be some/path/file.cc,
  # some/other/path/file.cc, some/path/file_platform.cc, some/path/file-suffix.h
  # etc. It's also possible that no special first include exists.
  # If the included file is some/path/file_platform.h the including file could
  # also be some/path/file_xxxtest_platform.h.
  including_file_base_name = test_file_tag_pattern.sub(
    '', input_api.os_path.basename(f.LocalPath()))

  for line in contents:
    line_num += 1
    if system_include_pattern.match(line):
      # No special first include -> process the line again along with normal
      # includes.
      line_num -= 1
      break
    match = custom_include_pattern.match(line)
    if match:
      match_dict = match.groupdict()
      header_basename = test_file_tag_pattern.sub(
        '', input_api.os_path.basename(match_dict['FILE'])).replace('.h', '')

      if header_basename not in including_file_base_name:
        # No special first include -> process the line again along with normal
        # includes.
        line_num -= 1
      break

  # Split into scopes: Each region between #if and #endif is its own scope.
  scopes = []
  current_scope = []
  for line in contents[line_num:]:
    line_num += 1
    if uncheckable_includes_pattern.match(line):
      continue
    if if_pattern.match(line):
      scopes.append(current_scope)
      current_scope = []
    elif ((system_include_pattern.match(line) or
           custom_include_pattern.match(line)) and
          not excluded_include_pattern.match(line)):
      current_scope.append((line_num, line))
  scopes.append(current_scope)

  for scope in scopes:
    warnings.extend(_CheckIncludeOrderForScope(scope, input_api, f.LocalPath(),
                                               changed_linenums))
  return warnings


def _CheckIncludeOrder(input_api, output_api):
  """Checks that the #include order is correct.

  1. The corresponding header for source files.
  2. C system files in alphabetical order
  3. C++ system files in alphabetical order
  4. Project's .h files in alphabetical order

  Each region separated by #if, #elif, #else, #endif, #define and #undef follows
  these rules separately.
  """
  def FileFilterIncludeOrder(affected_file):
    black_list = (_EXCLUDED_PATHS + input_api.DEFAULT_BLACK_LIST)
    return input_api.FilterSourceFile(affected_file, black_list=black_list)

  warnings = []
  for f in input_api.AffectedFiles(file_filter=FileFilterIncludeOrder):
    if f.LocalPath().endswith(('.cc', '.h', '.mm')):
      changed_linenums = set(line_num for line_num, _ in f.ChangedContents())
      warnings.extend(_CheckIncludeOrderInFile(input_api, f, changed_linenums))

  results = []
  if warnings:
    results.append(output_api.PresubmitPromptOrNotify(_INCLUDE_ORDER_WARNING,
                                                      warnings))
  return results


def _CheckForVersionControlConflictsInFile(input_api, f):
  pattern = input_api.re.compile('^(?:<<<<<<<|>>>>>>>) |^=======$')
  errors = []
  for line_num, line in f.ChangedContents():
    if f.LocalPath().endswith('.md'):
      # First-level headers in markdown look a lot like version control
      # conflict markers. http://daringfireball.net/projects/markdown/basics
      continue
    if pattern.match(line):
      errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))
  return errors


def _CheckForVersionControlConflicts(input_api, output_api):
  """Usually this is not intentional and will cause a compile failure."""
  errors = []
  for f in input_api.AffectedFiles():
    errors.extend(_CheckForVersionControlConflictsInFile(input_api, f))

  results = []
  if errors:
    results.append(output_api.PresubmitError(
      'Version control conflict markers found, please resolve.', errors))
  return results


def _CheckHardcodedGoogleHostsInLowerLayers(input_api, output_api):
  def FilterFile(affected_file):
    """Filter function for use with input_api.AffectedSourceFiles,
    below.  This filters out everything except non-test files from
    top-level directories that generally speaking should not hard-code
    service URLs (e.g. src/android_webview/, src/content/ and others).
    """
    return input_api.FilterSourceFile(
      affected_file,
      white_list=(r'^(android_webview|base|content|net)[\\\/].*', ),
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST))

  base_pattern = ('"[^"]*(google|googleapis|googlezip|googledrive|appspot)'
                  '\.(com|net)[^"]*"')
  comment_pattern = input_api.re.compile('//.*%s' % base_pattern)
  pattern = input_api.re.compile(base_pattern)
  problems = []  # items are (filename, line_number, line)
  for f in input_api.AffectedSourceFiles(FilterFile):
    for line_num, line in f.ChangedContents():
      if not comment_pattern.search(line) and pattern.search(line):
        problems.append((f.LocalPath(), line_num, line))

  if problems:
    return [output_api.PresubmitPromptOrNotify(
        'Most layers below src/chrome/ should not hardcode service URLs.\n'
        'Are you sure this is correct?',
        ['  %s:%d:  %s' % (
            problem[0], problem[1], problem[2]) for problem in problems])]
  else:
    return []


def _CheckNoAbbreviationInPngFileName(input_api, output_api):
  """Makes sure there are no abbreviations in the name of PNG files.
  The native_client_sdk directory is excluded because it has auto-generated PNG
  files for documentation.
  """
  errors = []
  white_list = (r'.*_[a-z]_.*\.png$|.*_[a-z]\.png$',)
  black_list = (r'^native_client_sdk[\\\/]',)
  file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=white_list, black_list=black_list)
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    errors.append('    %s' % f.LocalPath())

  results = []
  if errors:
    results.append(output_api.PresubmitError(
        'The name of PNG files should not have abbreviations. \n'
        'Use _hover.png, _center.png, instead of _h.png, _c.png.\n'
        'Contact oshima@chromium.org if you have questions.', errors))
  return results


def _FilesToCheckForIncomingDeps(re, changed_lines):
  """Helper method for _CheckAddedDepsHaveTargetApprovals. Returns
  a set of DEPS entries that we should look up.

  For a directory (rather than a specific filename) we fake a path to
  a specific filename by adding /DEPS. This is chosen as a file that
  will seldom or never be subject to per-file include_rules.
  """
  # We ignore deps entries on auto-generated directories.
  AUTO_GENERATED_DIRS = ['grit', 'jni']

  # This pattern grabs the path without basename in the first
  # parentheses, and the basename (if present) in the second. It
  # relies on the simple heuristic that if there is a basename it will
  # be a header file ending in ".h".
  pattern = re.compile(
      r"""['"]\+([^'"]+?)(/[a-zA-Z0-9_]+\.h)?['"].*""")
  results = set()
  for changed_line in changed_lines:
    m = pattern.match(changed_line)
    if m:
      path = m.group(1)
      if path.split('/')[0] not in AUTO_GENERATED_DIRS:
        if m.group(2):
          results.add('%s%s' % (path, m.group(2)))
        else:
          results.add('%s/DEPS' % path)
  return results


def _CheckAddedDepsHaveTargetApprovals(input_api, output_api):
  """When a dependency prefixed with + is added to a DEPS file, we
  want to make sure that the change is reviewed by an OWNER of the
  target file or directory, to avoid layering violations from being
  introduced. This check verifies that this happens.
  """
  changed_lines = set()

  file_filter = lambda f: not input_api.re.match(
      r"^third_party[\\\/]WebKit[\\\/].*", f.LocalPath())
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    filename = input_api.os_path.basename(f.LocalPath())
    if filename == 'DEPS':
      changed_lines |= set(line.strip()
                           for line_num, line
                           in f.ChangedContents())
  if not changed_lines:
    return []

  virtual_depended_on_files = _FilesToCheckForIncomingDeps(input_api.re,
                                                           changed_lines)
  if not virtual_depended_on_files:
    return []

  if input_api.is_committing:
    if input_api.tbr:
      return [output_api.PresubmitNotifyResult(
          '--tbr was specified, skipping OWNERS check for DEPS additions')]
    if input_api.dry_run:
      return [output_api.PresubmitNotifyResult(
          'This is a dry run, skipping OWNERS check for DEPS additions')]
    if not input_api.change.issue:
      return [output_api.PresubmitError(
          "DEPS approval by OWNERS check failed: this change has "
          "no Rietveld issue number, so we can't check it for approvals.")]
    output = output_api.PresubmitError
  else:
    output = output_api.PresubmitNotifyResult

  owners_db = input_api.owners_db
  owner_email, reviewers = (
      input_api.canned_checks.GetCodereviewOwnerAndReviewers(
        input_api,
        owners_db.email_regexp,
        approval_needed=input_api.is_committing))

  owner_email = owner_email or input_api.change.author_email

  reviewers_plus_owner = set(reviewers)
  if owner_email:
    reviewers_plus_owner.add(owner_email)
  missing_files = owners_db.files_not_covered_by(virtual_depended_on_files,
                                                 reviewers_plus_owner)

  # We strip the /DEPS part that was added by
  # _FilesToCheckForIncomingDeps to fake a path to a file in a
  # directory.
  def StripDeps(path):
    start_deps = path.rfind('/DEPS')
    if start_deps != -1:
      return path[:start_deps]
    else:
      return path
  unapproved_dependencies = ["'+%s'," % StripDeps(path)
                             for path in missing_files]

  if unapproved_dependencies:
    output_list = [
      output('Missing LGTM from OWNERS of dependencies added to DEPS:\n    %s' %
             '\n    '.join(sorted(unapproved_dependencies)))]
    if not input_api.is_committing:
      suggested_owners = owners_db.reviewers_for(missing_files, owner_email)
      output_list.append(output(
          'Suggested missing target path OWNERS:\n    %s' %
          '\n    '.join(suggested_owners or [])))
    return output_list

  return []


def _CheckSpamLogging(input_api, output_api):
  file_inclusion_pattern = r'.+%s' % _IMPLEMENTATION_EXTENSIONS
  black_list = (_EXCLUDED_PATHS +
                _TEST_CODE_EXCLUDED_PATHS +
                input_api.DEFAULT_BLACK_LIST +
                (r"^base[\\\/]logging\.h$",
                 r"^base[\\\/]logging\.cc$",
                 r"^chrome[\\\/]app[\\\/]chrome_main_delegate\.cc$",
                 r"^chrome[\\\/]browser[\\\/]chrome_browser_main\.cc$",
                 r"^chrome[\\\/]browser[\\\/]ui[\\\/]startup[\\\/]"
                     r"startup_browser_creator\.cc$",
                 r"^chrome[\\\/]installer[\\\/]setup[\\\/].*",
                 r"chrome[\\\/]browser[\\\/]diagnostics[\\\/]" +
                     r"diagnostics_writer\.cc$",
                 r"^chrome_elf[\\\/]dll_hash[\\\/]dll_hash_main\.cc$",
                 r"^chromecast[\\\/]",
                 r"^cloud_print[\\\/]",
                 r"^components[\\\/]html_viewer[\\\/]"
                     r"web_test_delegate_impl\.cc$",
                 # TODO(peter): Remove this exception. https://crbug.com/534537
                 r"^content[\\\/]browser[\\\/]notifications[\\\/]"
                     r"notification_event_dispatcher_impl\.cc$",
                 r"^content[\\\/]common[\\\/]gpu[\\\/]client[\\\/]"
                     r"gl_helper_benchmark\.cc$",
                 r"^courgette[\\\/]courgette_minimal_tool\.cc$",
                 r"^courgette[\\\/]courgette_tool\.cc$",
                 r"^extensions[\\\/]renderer[\\\/]logging_native_handler\.cc$",
                 r"^ipc[\\\/]ipc_logging\.cc$",
                 r"^native_client_sdk[\\\/]",
                 r"^remoting[\\\/]base[\\\/]logging\.h$",
                 r"^remoting[\\\/]host[\\\/].*",
                 r"^sandbox[\\\/]linux[\\\/].*",
                 r"^tools[\\\/]",
                 r"^ui[\\\/]aura[\\\/]bench[\\\/]bench_main\.cc$",
                 r"^ui[\\\/]ozone[\\\/]platform[\\\/]cast[\\\/]",
                 r"^storage[\\\/]browser[\\\/]fileapi[\\\/]" +
                     r"dump_file_system.cc$",))
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x, white_list=(file_inclusion_pattern,), black_list=black_list)

  log_info = []
  printf = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if input_api.re.search(r"\bD?LOG\s*\(\s*INFO\s*\)", contents):
      log_info.append(f.LocalPath())
    elif input_api.re.search(r"\bD?LOG_IF\s*\(\s*INFO\s*,", contents):
      log_info.append(f.LocalPath())

    if input_api.re.search(r"\bprintf\(", contents):
      printf.append(f.LocalPath())
    elif input_api.re.search(r"\bfprintf\((stdout|stderr)", contents):
      printf.append(f.LocalPath())

  if log_info:
    return [output_api.PresubmitError(
      'These files spam the console log with LOG(INFO):',
      items=log_info)]
  if printf:
    return [output_api.PresubmitError(
      'These files spam the console log with printf/fprintf:',
      items=printf)]
  return []


def _CheckForAnonymousVariables(input_api, output_api):
  """These types are all expected to hold locks while in scope and
     so should never be anonymous (which causes them to be immediately
     destroyed)."""
  they_who_must_be_named = [
    'base::AutoLock',
    'base::AutoReset',
    'base::AutoUnlock',
    'SkAutoAlphaRestore',
    'SkAutoBitmapShaderInstall',
    'SkAutoBlitterChoose',
    'SkAutoBounderCommit',
    'SkAutoCallProc',
    'SkAutoCanvasRestore',
    'SkAutoCommentBlock',
    'SkAutoDescriptor',
    'SkAutoDisableDirectionCheck',
    'SkAutoDisableOvalCheck',
    'SkAutoFree',
    'SkAutoGlyphCache',
    'SkAutoHDC',
    'SkAutoLockColors',
    'SkAutoLockPixels',
    'SkAutoMalloc',
    'SkAutoMaskFreeImage',
    'SkAutoMutexAcquire',
    'SkAutoPathBoundsUpdate',
    'SkAutoPDFRelease',
    'SkAutoRasterClipValidate',
    'SkAutoRef',
    'SkAutoTime',
    'SkAutoTrace',
    'SkAutoUnref',
  ]
  anonymous = r'(%s)\s*[({]' % '|'.join(they_who_must_be_named)
  # bad: base::AutoLock(lock.get());
  # not bad: base::AutoLock lock(lock.get());
  bad_pattern = input_api.re.compile(anonymous)
  # good: new base::AutoLock(lock.get())
  good_pattern = input_api.re.compile(r'\bnew\s*' + anonymous)
  errors = []

  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith(('.cc', '.h', '.inl', '.m', '.mm')):
      continue
    for linenum, line in f.ChangedContents():
      if bad_pattern.search(line) and not good_pattern.search(line):
        errors.append('%s:%d' % (f.LocalPath(), linenum))

  if errors:
    return [output_api.PresubmitError(
      'These lines create anonymous variables that need to be named:',
      items=errors)]
  return []


def _CheckCygwinShell(input_api, output_api):
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x, white_list=(r'.+\.(gyp|gypi)$',))
  cygwin_shell = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    for linenum, line in f.ChangedContents():
      if 'msvs_cygwin_shell' in line:
        cygwin_shell.append(f.LocalPath())
        break

  if cygwin_shell:
    return [output_api.PresubmitError(
      'These files should not use msvs_cygwin_shell (the default is 0):',
      items=cygwin_shell)]
  return []


def _CheckUserActionUpdate(input_api, output_api):
  """Checks if any new user action has been added."""
  if any('actions.xml' == input_api.os_path.basename(f) for f in
         input_api.LocalPaths()):
    # If actions.xml is already included in the changelist, the PRESUBMIT
    # for actions.xml will do a more complete presubmit check.
    return []

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm'))
  action_re = r'[^a-zA-Z]UserMetricsAction\("([^"]*)'
  current_actions = None
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      match = input_api.re.search(action_re, line)
      if match:
        # Loads contents in tools/metrics/actions/actions.xml to memory. It's
        # loaded only once.
        if not current_actions:
          with open('tools/metrics/actions/actions.xml') as actions_f:
            current_actions = actions_f.read()
        # Search for the matched user action name in |current_actions|.
        for action_name in match.groups():
          action = 'name="{0}"'.format(action_name)
          if action not in current_actions:
            return [output_api.PresubmitPromptWarning(
              'File %s line %d: %s is missing in '
              'tools/metrics/actions/actions.xml. Please run '
              'tools/metrics/actions/extract_actions.py to update.'
              % (f.LocalPath(), line_num, action_name))]
  return []


def _GetJSONParseError(input_api, filename, eat_comments=True):
  try:
    contents = input_api.ReadFile(filename)
    if eat_comments:
      import sys
      original_sys_path = sys.path
      try:
        sys.path = sys.path + [input_api.os_path.join(
            input_api.PresubmitLocalPath(),
            'tools', 'json_comment_eater')]
        import json_comment_eater
      finally:
        sys.path = original_sys_path
      contents = json_comment_eater.Nom(contents)

    input_api.json.loads(contents)
  except ValueError as e:
    return e
  return None


def _GetIDLParseError(input_api, filename):
  try:
    contents = input_api.ReadFile(filename)
    idl_schema = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        'tools', 'json_schema_compiler', 'idl_schema.py')
    process = input_api.subprocess.Popen(
        [input_api.python_executable, idl_schema],
        stdin=input_api.subprocess.PIPE,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE,
        universal_newlines=True)
    (_, error) = process.communicate(input=contents)
    return error or None
  except ValueError as e:
    return e


def _CheckParseErrors(input_api, output_api):
  """Check that IDL and JSON files do not contain syntax errors."""
  actions = {
    '.idl': _GetIDLParseError,
    '.json': _GetJSONParseError,
  }
  # These paths contain test data and other known invalid JSON files.
  excluded_patterns = [
    r'test[\\\/]data[\\\/]',
    r'^components[\\\/]policy[\\\/]resources[\\\/]policy_templates\.json$',
  ]
  # Most JSON files are preprocessed and support comments, but these do not.
  json_no_comments_patterns = [
    r'^testing[\\\/]',
  ]
  # Only run IDL checker on files in these directories.
  idl_included_patterns = [
    r'^chrome[\\\/]common[\\\/]extensions[\\\/]api[\\\/]',
    r'^extensions[\\\/]common[\\\/]api[\\\/]',
  ]

  def get_action(affected_file):
    filename = affected_file.LocalPath()
    return actions.get(input_api.os_path.splitext(filename)[1])

  def MatchesFile(patterns, path):
    for pattern in patterns:
      if input_api.re.search(pattern, path):
        return True
    return False

  def FilterFile(affected_file):
    action = get_action(affected_file)
    if not action:
      return False
    path = affected_file.LocalPath()

    if MatchesFile(excluded_patterns, path):
      return False

    if (action == _GetIDLParseError and
        not MatchesFile(idl_included_patterns, path)):
      return False
    return True

  results = []
  for affected_file in input_api.AffectedFiles(
      file_filter=FilterFile, include_deletes=False):
    action = get_action(affected_file)
    kwargs = {}
    if (action == _GetJSONParseError and
        MatchesFile(json_no_comments_patterns, affected_file.LocalPath())):
      kwargs['eat_comments'] = False
    parse_error = action(input_api,
                         affected_file.AbsoluteLocalPath(),
                         **kwargs)
    if parse_error:
      results.append(output_api.PresubmitError('%s could not be parsed: %s' %
          (affected_file.LocalPath(), parse_error)))
  return results


def _CheckJavaStyle(input_api, output_api):
  """Runs checkstyle on changed java files and returns errors if any exist."""
  import sys
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools', 'android', 'checkstyle')]
    import checkstyle
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  return checkstyle.RunCheckstyle(
      input_api, output_api, 'tools/android/checkstyle/chromium-style-5.0.xml',
      black_list=_EXCLUDED_PATHS + input_api.DEFAULT_BLACK_LIST)


def _CheckIpcOwners(input_api, output_api):
  """Checks that affected files involving IPC have an IPC OWNERS rule.

  Whether or not a file affects IPC is determined by a simple whitelist of
  filename patterns."""
  file_patterns = [
      '*_messages.cc',
      '*_messages*.h',
      '*_param_traits*.*',
      '*.mojom',
      '*_struct_traits*.*',
      '*_type_converter*.*',
      # Blink uses a different file naming convention
      '*StructTraits*.*',
      '*TypeConverter*.*',
  ]

  # Dictionary mapping an OWNERS file path to Patterns.
  # Patterns is a dictionary mapping glob patterns (suitable for use in per-file
  # rules ) to a PatternEntry.
  # PatternEntry is a dictionary with two keys:
  # - 'files': the files that are matched by this pattern
  # - 'rules': the per-file rules needed for this pattern
  # For example, if we expect OWNERS file to contain rules for *.mojom and
  # *_struct_traits*.*, Patterns might look like this:
  # {
  #   '*.mojom': {
  #     'files': ...,
  #     'rules': [
  #       'per-file *.mojom=set noparent',
  #       'per-file *.mojom=file://ipc/SECURITY_OWNERS',
  #     ],
  #   },
  #   '*_struct_traits*.*': {
  #     'files': ...,
  #     'rules': [
  #       'per-file *_struct_traits*.*=set noparent',
  #       'per-file *_struct_traits*.*=file://ipc/SECURITY_OWNERS',
  #     ],
  #   },
  # }
  to_check = {}

  # Iterate through the affected files to see what we actually need to check
  # for. We should only nag patch authors about per-file rules if a file in that
  # directory would match that pattern. If a directory only contains *.mojom
  # files and no *_messages*.h files, we should only nag about rules for
  # *.mojom files.
  for f in input_api.change.AffectedFiles(include_deletes=False):
    for pattern in file_patterns:
      if input_api.fnmatch.fnmatch(
          input_api.os_path.basename(f.LocalPath()), pattern):
        owners_file = input_api.os_path.join(
            input_api.os_path.dirname(f.LocalPath()), 'OWNERS')
        if owners_file not in to_check:
          to_check[owners_file] = {}
        if pattern not in to_check[owners_file]:
          to_check[owners_file][pattern] = {
              'files': [],
              'rules': [
                  'per-file %s=set noparent' % pattern,
                  'per-file %s=file://ipc/SECURITY_OWNERS' % pattern,
              ]
          }
        to_check[owners_file][pattern]['files'].append(f)
        break

  # Now go through the OWNERS files we collected, filtering out rules that are
  # already present in that OWNERS file.
  for owners_file, patterns in to_check.iteritems():
    try:
      with file(owners_file) as f:
        lines = set(f.read().splitlines())
        for entry in patterns.itervalues():
          entry['rules'] = [rule for rule in entry['rules'] if rule not in lines
                           ]
    except IOError:
      # No OWNERS file, so all the rules are definitely missing.
      continue

  # All the remaining lines weren't found in OWNERS files, so emit an error.
  errors = []
  for owners_file, patterns in to_check.iteritems():
    missing_lines = []
    files = []
    for pattern, entry in patterns.iteritems():
      missing_lines.extend(entry['rules'])
      files.extend(['  %s' % f.LocalPath() for f in entry['files']])
    if missing_lines:
      errors.append(
          '%s is missing the following lines:\n\n%s\n\nfor changed files:\n%s' %
          (owners_file, '\n'.join(missing_lines), '\n'.join(files)))

  results = []
  if errors:
    results.append(output_api.PresubmitError(
        'Found changes to IPC files without a security OWNER!',
        long_text='\n\n'.join(errors)))

  return results


def _CheckAndroidToastUsage(input_api, output_api):
  """Checks that code uses org.chromium.ui.widget.Toast instead of
     android.widget.Toast (Chromium Toast doesn't force hardware
     acceleration on low-end devices, saving memory).
  """
  toast_import_pattern = input_api.re.compile(
      r'^import android\.widget\.Toast;$')

  errors = []

  sources = lambda affected_file: input_api.FilterSourceFile(
      affected_file,
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r'^chromecast[\\\/].*',
                   r'^remoting[\\\/].*')),
      white_list=(r'.*\.java$',))

  for f in input_api.AffectedSourceFiles(sources):
    for line_num, line in f.ChangedContents():
      if toast_import_pattern.search(line):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitError(
        'android.widget.Toast usage is detected. Android toasts use hardware'
        ' acceleration, and can be\ncostly on low-end devices. Please use'
        ' org.chromium.ui.widget.Toast instead.\n'
        'Contact dskiba@chromium.org if you have any questions.',
        errors))

  return results


def _CheckAndroidCrLogUsage(input_api, output_api):
  """Checks that new logs using org.chromium.base.Log:
    - Are using 'TAG' as variable name for the tags (warn)
    - Are using a tag that is shorter than 20 characters (error)
  """

  # Do not check format of logs in //chrome/android/webapk because
  # //chrome/android/webapk cannot depend on //base
  cr_log_check_excluded_paths = [
    r"^chrome[\\\/]android[\\\/]webapk[\\\/].*",
  ]

  cr_log_import_pattern = input_api.re.compile(
      r'^import org\.chromium\.base\.Log;$', input_api.re.MULTILINE)
  class_in_base_pattern = input_api.re.compile(
      r'^package org\.chromium\.base;$', input_api.re.MULTILINE)
  has_some_log_import_pattern = input_api.re.compile(
      r'^import .*\.Log;$', input_api.re.MULTILINE)
  # Extract the tag from lines like `Log.d(TAG, "*");` or `Log.d("TAG", "*");`
  log_call_pattern = input_api.re.compile(r'^\s*Log\.\w\((?P<tag>\"?\w+\"?)\,')
  log_decl_pattern = input_api.re.compile(
      r'^\s*private static final String TAG = "(?P<name>(.*))";',
      input_api.re.MULTILINE)

  REF_MSG = ('See docs/android_logging.md '
            'or contact dgn@chromium.org for more info.')
  sources = lambda x: input_api.FilterSourceFile(x, white_list=(r'.*\.java$',),
      black_list=cr_log_check_excluded_paths)

  tag_decl_errors = []
  tag_length_errors = []
  tag_errors = []
  tag_with_dot_errors = []
  util_log_errors = []

  for f in input_api.AffectedSourceFiles(sources):
    file_content = input_api.ReadFile(f)
    has_modified_logs = False

    # Per line checks
    if (cr_log_import_pattern.search(file_content) or
        (class_in_base_pattern.search(file_content) and
            not has_some_log_import_pattern.search(file_content))):
      # Checks to run for files using cr log
      for line_num, line in f.ChangedContents():

        # Check if the new line is doing some logging
        match = log_call_pattern.search(line)
        if match:
          has_modified_logs = True

          # Make sure it uses "TAG"
          if not match.group('tag') == 'TAG':
            tag_errors.append("%s:%d" % (f.LocalPath(), line_num))
    else:
      # Report non cr Log function calls in changed lines
      for line_num, line in f.ChangedContents():
        if log_call_pattern.search(line):
          util_log_errors.append("%s:%d" % (f.LocalPath(), line_num))

    # Per file checks
    if has_modified_logs:
      # Make sure the tag is using the "cr" prefix and is not too long
      match = log_decl_pattern.search(file_content)
      tag_name = match.group('name') if match else None
      if not tag_name:
        tag_decl_errors.append(f.LocalPath())
      elif len(tag_name) > 20:
        tag_length_errors.append(f.LocalPath())
      elif '.' in tag_name:
        tag_with_dot_errors.append(f.LocalPath())

  results = []
  if tag_decl_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Please define your tags using the suggested format: .\n'
        '"private static final String TAG = "<package tag>".\n'
        'They will be prepended with "cr_" automatically.\n' + REF_MSG,
        tag_decl_errors))

  if tag_length_errors:
    results.append(output_api.PresubmitError(
        'The tag length is restricted by the system to be at most '
        '20 characters.\n' + REF_MSG,
        tag_length_errors))

  if tag_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Please use a variable named "TAG" for your log tags.\n' + REF_MSG,
        tag_errors))

  if util_log_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Please use org.chromium.base.Log for new logs.\n' + REF_MSG,
        util_log_errors))

  if tag_with_dot_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Dot in log tags cause them to be elided in crash reports.\n' + REF_MSG,
        tag_with_dot_errors))

  return results


def _CheckAndroidNewMdpiAssetLocation(input_api, output_api):
  """Checks if MDPI assets are placed in a correct directory."""
  file_filter = lambda f: (f.LocalPath().endswith('.png') and
                           ('/res/drawable/' in f.LocalPath() or
                            '/res/drawable-ldrtl/' in f.LocalPath()))
  errors = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    errors.append('    %s' % f.LocalPath())

  results = []
  if errors:
    results.append(output_api.PresubmitError(
        'MDPI assets should be placed in /res/drawable-mdpi/ or '
        '/res/drawable-ldrtl-mdpi/\ninstead of /res/drawable/ and'
        '/res/drawable-ldrtl/.\n'
        'Contact newt@chromium.org if you have questions.', errors))
  return results


class PydepsChecker(object):
  def __init__(self, input_api, pydeps_files):
    self._file_cache = {}
    self._input_api = input_api
    self._pydeps_files = pydeps_files

  def _LoadFile(self, path):
    """Returns the list of paths within a .pydeps file relative to //."""
    if path not in self._file_cache:
      with open(path) as f:
        self._file_cache[path] = f.read()
    return self._file_cache[path]

  def _ComputeNormalizedPydepsEntries(self, pydeps_path):
    """Returns an interable of paths within the .pydep, relativized to //."""
    os_path = self._input_api.os_path
    pydeps_dir = os_path.dirname(pydeps_path)
    entries = (l.rstrip() for l in self._LoadFile(pydeps_path).splitlines()
               if not l.startswith('*'))
    return (os_path.normpath(os_path.join(pydeps_dir, e)) for e in entries)

  def _CreateFilesToPydepsMap(self):
    """Returns a map of local_path -> list_of_pydeps."""
    ret = {}
    for pydep_local_path in self._pydeps_files:
      for path in self._ComputeNormalizedPydepsEntries(pydep_local_path):
        ret.setdefault(path, []).append(pydep_local_path)
    return ret

  def ComputeAffectedPydeps(self):
    """Returns an iterable of .pydeps files that might need regenerating."""
    affected_pydeps = set()
    file_to_pydeps_map = None
    for f in self._input_api.AffectedFiles(include_deletes=True):
      local_path = f.LocalPath()
      if local_path  == 'DEPS':
        return self._pydeps_files
      elif local_path.endswith('.pydeps'):
        if local_path in self._pydeps_files:
          affected_pydeps.add(local_path)
      elif local_path.endswith('.py'):
        if file_to_pydeps_map is None:
          file_to_pydeps_map = self._CreateFilesToPydepsMap()
        affected_pydeps.update(file_to_pydeps_map.get(local_path, ()))
    return affected_pydeps

  def DetermineIfStale(self, pydeps_path):
    """Runs print_python_deps.py to see if the files is stale."""
    old_pydeps_data = self._LoadFile(pydeps_path).splitlines()
    cmd = old_pydeps_data[1][1:].strip()
    new_pydeps_data = self._input_api.subprocess.check_output(
        cmd  + ' --output ""', shell=True)
    if old_pydeps_data[2:] != new_pydeps_data.splitlines()[2:]:
      return cmd


def _CheckPydepsNeedsUpdating(input_api, output_api, checker_for_tests=None):
  """Checks if a .pydeps file needs to be regenerated."""
  # This check is mainly for Android, and involves paths not only in the
  # PRESUBMIT.py, but also in the .pydeps files. It doesn't work on Windows and
  # Mac, so skip it on other platforms.
  if input_api.platform != 'linux2':
    return []
  # TODO(agrieve): Update when there's a better way to detect this: crbug/570091
  is_android = input_api.os_path.exists('third_party/android_tools')
  pydeps_files = _ALL_PYDEPS_FILES if is_android else _GENERIC_PYDEPS_FILES
  results = []
  # First, check for new / deleted .pydeps.
  for f in input_api.AffectedFiles(include_deletes=True):
    if f.LocalPath().endswith('.pydeps'):
      if f.Action() == 'D' and f.LocalPath() in _ALL_PYDEPS_FILES:
        results.append(output_api.PresubmitError(
            'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py to '
            'remove %s' % f.LocalPath()))
      elif f.Action() != 'D' and f.LocalPath() not in _ALL_PYDEPS_FILES:
        results.append(output_api.PresubmitError(
            'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py to '
            'include %s' % f.LocalPath()))

  if results:
    return results

  checker = checker_for_tests or PydepsChecker(input_api, pydeps_files)

  for pydep_path in checker.ComputeAffectedPydeps():
    try:
      cmd = checker.DetermineIfStale(pydep_path)
      if cmd:
        results.append(output_api.PresubmitError(
            'File is stale: %s\nTo regenerate, run:\n\n    %s' %
            (pydep_path, cmd)))
    except input_api.subprocess.CalledProcessError as error:
      return [output_api.PresubmitError('Error running: %s' % error.cmd,
          long_text=error.output)]

  return results


def _CheckForCopyrightedCode(input_api, output_api):
  """Verifies that newly added code doesn't contain copyrighted material
  and is properly licensed under the standard Chromium license.

  As there can be false positives, we maintain a whitelist file. This check
  also verifies that the whitelist file is up to date.
  """
  import sys
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools')]
    from copyright_scanner import copyright_scanner
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  return copyright_scanner.ScanAtPresubmit(input_api, output_api)


def _CheckSingletonInHeaders(input_api, output_api):
  """Checks to make sure no header files have |Singleton<|."""
  def FileFilter(affected_file):
    # It's ok for base/memory/singleton.h to have |Singleton<|.
    black_list = (_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r"^base[\\\/]memory[\\\/]singleton\.h$",))
    return input_api.FilterSourceFile(affected_file, black_list=black_list)

  pattern = input_api.re.compile(r'(?<!class\sbase::)Singleton\s*<')
  files = []
  for f in input_api.AffectedSourceFiles(FileFilter):
    if (f.LocalPath().endswith('.h') or f.LocalPath().endswith('.hxx') or
        f.LocalPath().endswith('.hpp') or f.LocalPath().endswith('.inl')):
      contents = input_api.ReadFile(f)
      for line in contents.splitlines(False):
        if (not line.lstrip().startswith('//') and # Strip C++ comment.
            pattern.search(line)):
          files.append(f)
          break

  if files:
    return [output_api.PresubmitError(
        'Found base::Singleton<T> in the following header files.\n' +
        'Please move them to an appropriate source file so that the ' +
        'template gets instantiated in a single compilation unit.',
        files) ]
  return []


def _CheckNoDeprecatedCompiledResourcesGYP(input_api, output_api):
  """Checks for old style compiled_resources.gyp files."""
  is_compiled_resource = lambda fp: fp.endswith('compiled_resources.gyp')

  added_compiled_resources = filter(is_compiled_resource, [
    f.LocalPath() for f in input_api.AffectedFiles() if f.Action() == 'A'
  ])

  if not added_compiled_resources:
    return []

  return [output_api.PresubmitError(
      "Found new compiled_resources.gyp files:\n%s\n\n"
      "compiled_resources.gyp files are deprecated,\n"
      "please use compiled_resources2.gyp instead:\n"
      "https://chromium.googlesource.com/chromium/src/+/master/docs/closure_compilation.md"
      %
      "\n".join(added_compiled_resources))]


_DEPRECATED_CSS = [
  # Values
  ( "-webkit-box", "flex" ),
  ( "-webkit-inline-box", "inline-flex" ),
  ( "-webkit-flex", "flex" ),
  ( "-webkit-inline-flex", "inline-flex" ),
  ( "-webkit-min-content", "min-content" ),
  ( "-webkit-max-content", "max-content" ),

  # Properties
  ( "-webkit-background-clip", "background-clip" ),
  ( "-webkit-background-origin", "background-origin" ),
  ( "-webkit-background-size", "background-size" ),
  ( "-webkit-box-shadow", "box-shadow" ),

  # Functions
  ( "-webkit-gradient", "gradient" ),
  ( "-webkit-repeating-gradient", "repeating-gradient" ),
  ( "-webkit-linear-gradient", "linear-gradient" ),
  ( "-webkit-repeating-linear-gradient", "repeating-linear-gradient" ),
  ( "-webkit-radial-gradient", "radial-gradient" ),
  ( "-webkit-repeating-radial-gradient", "repeating-radial-gradient" ),
]

def _CheckNoDeprecatedCSS(input_api, output_api):
  """ Make sure that we don't use deprecated CSS
      properties, functions or values. Our external
      documentation and iOS CSS for dom distiller
      (reader mode) are ignored by the hooks as it
      needs to be consumed by WebKit. """
  results = []
  file_inclusion_pattern = (r".+\.css$",)
  black_list = (_EXCLUDED_PATHS +
                _TEST_CODE_EXCLUDED_PATHS +
                input_api.DEFAULT_BLACK_LIST +
                (r"^chrome/common/extensions/docs",
                 r"^chrome/docs",
                 r"^components/dom_distiller/core/css/distilledpage_ios.css",
                 r"^components/flags_ui/resources/apple_flags.css",
                 r"^components/neterror/resources/neterror.css",
                 r"^native_client_sdk"))
  file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=file_inclusion_pattern, black_list=black_list)
  for fpath in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in fpath.ChangedContents():
      for (deprecated_value, value) in _DEPRECATED_CSS:
        if deprecated_value in line:
          results.append(output_api.PresubmitError(
              "%s:%d: Use of deprecated CSS %s, use %s instead" %
              (fpath.LocalPath(), line_num, deprecated_value, value)))
  return results


_DEPRECATED_JS = [
  ( "__lookupGetter__", "Object.getOwnPropertyDescriptor" ),
  ( "__defineGetter__", "Object.defineProperty" ),
  ( "__defineSetter__", "Object.defineProperty" ),
]

def _CheckNoDeprecatedJS(input_api, output_api):
  """Make sure that we don't use deprecated JS in Chrome code."""
  results = []
  file_inclusion_pattern = (r".+\.js$",)  # TODO(dbeam): .html?
  black_list = (_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                input_api.DEFAULT_BLACK_LIST)
  file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=file_inclusion_pattern, black_list=black_list)
  for fpath in input_api.AffectedFiles(file_filter=file_filter):
    for lnum, line in fpath.ChangedContents():
      for (deprecated, replacement) in _DEPRECATED_JS:
        if deprecated in line:
          results.append(output_api.PresubmitError(
              "%s:%d: Use of deprecated JS %s, use %s instead" %
              (fpath.LocalPath(), lnum, deprecated, replacement)))
  return results


def _AndroidSpecificOnUploadChecks(input_api, output_api):
  """Groups checks that target android code."""
  results = []
  results.extend(_CheckAndroidCrLogUsage(input_api, output_api))
  results.extend(_CheckAndroidNewMdpiAssetLocation(input_api, output_api))
  results.extend(_CheckAndroidToastUsage(input_api, output_api))
  return results


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api,
      excluded_paths=_EXCLUDED_PATHS + _TESTRUNNER_PATHS))
  results.extend(_CheckAuthorizedAuthor(input_api, output_api))
  results.extend(
      _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api))
  results.extend(_CheckNoIOStreamInHeaders(input_api, output_api))
  results.extend(_CheckNoUNIT_TESTInSourceFiles(input_api, output_api))
  results.extend(_CheckDCHECK_IS_ONHasBraces(input_api, output_api))
  results.extend(_CheckNoNewWStrings(input_api, output_api))
  results.extend(_CheckNoDEPSGIT(input_api, output_api))
  results.extend(_CheckNoBannedFunctions(input_api, output_api))
  results.extend(_CheckNoPragmaOnce(input_api, output_api))
  results.extend(_CheckNoTrinaryTrueFalse(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(_CheckFilePermissions(input_api, output_api))
  results.extend(_CheckNoAuraWindowPropertyHInHeaders(input_api, output_api))
  results.extend(_CheckIncludeOrder(input_api, output_api))
  results.extend(_CheckForVersionControlConflicts(input_api, output_api))
  results.extend(_CheckPatchFiles(input_api, output_api))
  results.extend(_CheckHardcodedGoogleHostsInLowerLayers(input_api, output_api))
  results.extend(_CheckNoAbbreviationInPngFileName(input_api, output_api))
  results.extend(_CheckForInvalidOSMacros(input_api, output_api))
  results.extend(_CheckForInvalidIfDefinedMacros(input_api, output_api))
  results.extend(_CheckFlakyTestUsage(input_api, output_api))
  results.extend(_CheckAddedDepsHaveTargetApprovals(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckChangeHasNoTabs(
          input_api,
          output_api,
          source_file_filter=lambda x: x.LocalPath().endswith('.grd')))
  results.extend(_CheckSpamLogging(input_api, output_api))
  results.extend(_CheckForAnonymousVariables(input_api, output_api))
  results.extend(_CheckCygwinShell(input_api, output_api))
  results.extend(_CheckUserActionUpdate(input_api, output_api))
  results.extend(_CheckNoDeprecatedCSS(input_api, output_api))
  results.extend(_CheckNoDeprecatedJS(input_api, output_api))
  results.extend(_CheckParseErrors(input_api, output_api))
  results.extend(_CheckForIPCRules(input_api, output_api))
  results.extend(_CheckForCopyrightedCode(input_api, output_api))
  results.extend(_CheckForWindowsLineEndings(input_api, output_api))
  results.extend(_CheckSingletonInHeaders(input_api, output_api))
  results.extend(_CheckNoDeprecatedCompiledResourcesGYP(input_api, output_api))
  results.extend(_CheckPydepsNeedsUpdating(input_api, output_api))
  results.extend(_CheckJavaStyle(input_api, output_api))
  results.extend(_CheckIpcOwners(input_api, output_api))

  if any('PRESUBMIT.py' == f.LocalPath() for f in input_api.AffectedFiles()):
    results.extend(input_api.canned_checks.RunUnitTestsInDirectory(
        input_api, output_api,
        input_api.PresubmitLocalPath(),
        whitelist=[r'^PRESUBMIT_test\.py$']))
  return results


def _CheckAuthorizedAuthor(input_api, output_api):
  """For non-googler/chromites committers, verify the author's email address is
  in AUTHORS.
  """
  author = input_api.change.author_email
  if not author:
    input_api.logging.info('No author, skipping AUTHOR check')
    return []
  authors_path = input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'AUTHORS')
  valid_authors = (
      input_api.re.match(r'[^#]+\s+\<(.+?)\>\s*$', line)
      for line in open(authors_path))
  valid_authors = [item.group(1).lower() for item in valid_authors if item]
  if not any(input_api.fnmatch.fnmatch(author.lower(), valid)
             for valid in valid_authors):
    input_api.logging.info('Valid authors are %s', ', '.join(valid_authors))
    return [output_api.PresubmitPromptWarning(
        ('%s is not in AUTHORS file. If you are a new contributor, please visit'
        '\n'
        'http://www.chromium.org/developers/contributing-code and read the '
        '"Legal" section\n'
        'If you are a chromite, verify the contributor signed the CLA.') %
        author)]
  return []


def _CheckPatchFiles(input_api, output_api):
  problems = [f.LocalPath() for f in input_api.AffectedFiles()
      if f.LocalPath().endswith(('.orig', '.rej'))]
  if problems:
    return [output_api.PresubmitError(
        "Don't commit .rej and .orig files.", problems)]
  else:
    return []


def _DidYouMeanOSMacro(bad_macro):
  try:
    return {'A': 'OS_ANDROID',
            'B': 'OS_BSD',
            'C': 'OS_CHROMEOS',
            'F': 'OS_FREEBSD',
            'L': 'OS_LINUX',
            'M': 'OS_MACOSX',
            'N': 'OS_NACL',
            'O': 'OS_OPENBSD',
            'P': 'OS_POSIX',
            'S': 'OS_SOLARIS',
            'W': 'OS_WIN'}[bad_macro[3].upper()]
  except KeyError:
    return ''


def _CheckForInvalidOSMacrosInFile(input_api, f):
  """Check for sensible looking, totally invalid OS macros."""
  preprocessor_statement = input_api.re.compile(r'^\s*#')
  os_macro = input_api.re.compile(r'defined\((OS_[^)]+)\)')
  results = []
  for lnum, line in f.ChangedContents():
    if preprocessor_statement.search(line):
      for match in os_macro.finditer(line):
        if not match.group(1) in _VALID_OS_MACROS:
          good = _DidYouMeanOSMacro(match.group(1))
          did_you_mean = ' (did you mean %s?)' % good if good else ''
          results.append('    %s:%d %s%s' % (f.LocalPath(),
                                             lnum,
                                             match.group(1),
                                             did_you_mean))
  return results


def _CheckForInvalidOSMacros(input_api, output_api):
  """Check all affected files for invalid OS macros."""
  bad_macros = []
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith(('.py', '.js', '.html', '.css', '.md')):
      bad_macros.extend(_CheckForInvalidOSMacrosInFile(input_api, f))

  if not bad_macros:
    return []

  return [output_api.PresubmitError(
      'Possibly invalid OS macro[s] found. Please fix your code\n'
      'or add your macro to src/PRESUBMIT.py.', bad_macros)]


def _CheckForInvalidIfDefinedMacrosInFile(input_api, f):
  """Check all affected files for invalid "if defined" macros."""
  ALWAYS_DEFINED_MACROS = (
      "TARGET_CPU_PPC",
      "TARGET_CPU_PPC64",
      "TARGET_CPU_68K",
      "TARGET_CPU_X86",
      "TARGET_CPU_ARM",
      "TARGET_CPU_MIPS",
      "TARGET_CPU_SPARC",
      "TARGET_CPU_ALPHA",
      "TARGET_IPHONE_SIMULATOR",
      "TARGET_OS_EMBEDDED",
      "TARGET_OS_IPHONE",
      "TARGET_OS_MAC",
      "TARGET_OS_UNIX",
      "TARGET_OS_WIN32",
  )
  ifdef_macro = input_api.re.compile(r'^\s*#.*(?:ifdef\s|defined\()([^\s\)]+)')
  results = []
  for lnum, line in f.ChangedContents():
    for match in ifdef_macro.finditer(line):
      if match.group(1) in ALWAYS_DEFINED_MACROS:
        always_defined = ' %s is always defined. ' % match.group(1)
        did_you_mean = 'Did you mean \'#if %s\'?' % match.group(1)
        results.append('    %s:%d %s\n\t%s' % (f.LocalPath(),
                                               lnum,
                                               always_defined,
                                               did_you_mean))
  return results


def _CheckForInvalidIfDefinedMacros(input_api, output_api):
  """Check all affected files for invalid "if defined" macros."""
  bad_macros = []
  for f in input_api.AffectedFiles():
    if f.LocalPath().endswith(('.h', '.c', '.cc', '.m', '.mm')):
      bad_macros.extend(_CheckForInvalidIfDefinedMacrosInFile(input_api, f))

  if not bad_macros:
    return []

  return [output_api.PresubmitError(
      'Found ifdef check on always-defined macro[s]. Please fix your code\n'
      'or check the list of ALWAYS_DEFINED_MACROS in src/PRESUBMIT.py.',
      bad_macros)]


def _CheckForIPCRules(input_api, output_api):
  """Check for same IPC rules described in
  http://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc
  """
  base_pattern = r'IPC_ENUM_TRAITS\('
  inclusion_pattern = input_api.re.compile(r'(%s)' % base_pattern)
  comment_pattern = input_api.re.compile(r'//.*(%s)' % base_pattern)

  problems = []
  for f in input_api.AffectedSourceFiles(None):
    local_path = f.LocalPath()
    if not local_path.endswith('.h'):
      continue
    for line_number, line in f.ChangedContents():
      if inclusion_pattern.search(line) and not comment_pattern.search(line):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptWarning(
        _IPC_ENUM_TRAITS_DEPRECATED, problems)]
  else:
    return []


def _CheckForWindowsLineEndings(input_api, output_api):
  """Check source code and known ascii text files for Windows style line
  endings.
  """
  known_text_files = r'.*\.(txt|html|htm|mhtml|py|gyp|gypi|gn|isolate)$'

  file_inclusion_pattern = (
    known_text_files,
    r'.+%s' % _IMPLEMENTATION_EXTENSIONS
  )

  filter = lambda f: input_api.FilterSourceFile(
    f, white_list=file_inclusion_pattern, black_list=None)
  files = [f.LocalPath() for f in
           input_api.AffectedSourceFiles(filter)]

  problems = []

  for file in files:
    fp = open(file, 'r')
    for line in fp:
      if line.endswith('\r\n'):
        problems.append(file)
        break
    fp.close()

  if problems:
    return [output_api.PresubmitPromptWarning('Are you sure that you want '
        'these files to contain Windows style line endings?\n' +
        '\n'.join(problems))]

  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(_CheckValidHostsInDEPS(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckGNFormatted(input_api, output_api))
  results.extend(_CheckUmaHistogramChanges(input_api, output_api))
  results.extend(_AndroidSpecificOnUploadChecks(input_api, output_api))
  return results


def GetTryServerMasterForBot(bot):
  """Returns the Try Server master for the given bot.

  It tries to guess the master from the bot name, but may still fail
  and return None.  There is no longer a default master.
  """
  # Potentially ambiguous bot names are listed explicitly.
  master_map = {
      'chromium_presubmit': 'tryserver.chromium.linux',
      'tools_build_presubmit': 'tryserver.chromium.linux',
  }
  master = master_map.get(bot)
  if not master:
    if 'android' in bot:
      master = 'tryserver.chromium.android'
    elif 'linux' in bot or 'presubmit' in bot:
      master = 'tryserver.chromium.linux'
    elif 'win' in bot:
      master = 'tryserver.chromium.win'
    elif 'mac' in bot or 'ios' in bot:
      master = 'tryserver.chromium.mac'
  return master


def GetDefaultTryConfigs(bots):
  """Returns a list of ('bot', set(['tests']), filtered by [bots].
  """

  builders_and_tests = dict((bot, set(['defaulttests'])) for bot in bots)

  # Build up the mapping from tryserver master to bot/test.
  out = dict()
  for bot, tests in builders_and_tests.iteritems():
    out.setdefault(GetTryServerMasterForBot(bot), {})[bot] = tests
  return out


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  # Make sure the tree is 'open'.
  results.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api,
      output_api,
      json_url='http://chromium-status.appspot.com/current?format=json'))

  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
  return results
