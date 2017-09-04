# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import os
import re
import sys


_EXCLUDED_PATHS = (
    # LayoutTests/imported is excluded because these files are automatically
    # imported, so we do not have direct control over their content.
    r'^third_party[\\\/]WebKit[\\\/]LayoutTests[\\\/]imported[\\\/].*',
)


def _CheckForNonBlinkVariantMojomIncludes(input_api, output_api):
    pattern = input_api.re.compile(r'#include\s+.+\.mojom(.*)\.h[>"]')
    errors = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            m = pattern.match(line)
            if m and m.group(1) != '-blink' and m.group(1) != '-shared':
                errors.append('    %s:%d %s' % (
                    f.LocalPath(), line_num, line))

    results = []
    if errors:
        results.append(output_api.PresubmitError(
            'Files that include non-Blink variant mojoms found:', errors))
    return results


def _CheckForVersionControlConflictsInFile(input_api, f):
    pattern = input_api.re.compile('^(?:<<<<<<<|>>>>>>>) |^=======$')
    errors = []
    for line_num, line in f.ChangedContents():
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


def _CheckWatchlist(input_api, output_api):
    """Check that the WATCHLIST file parses correctly."""
    errors = []
    for f in input_api.AffectedFiles():
        if f.LocalPath() != 'WATCHLISTS':
            continue
        import StringIO
        import logging
        import watchlists

        log_buffer = StringIO.StringIO()
        log_handler = logging.StreamHandler(log_buffer)
        log_handler.setFormatter(
            logging.Formatter('%(levelname)s: %(message)s'))
        logger = logging.getLogger()
        logger.addHandler(log_handler)

        wl = watchlists.Watchlists(input_api.change.RepositoryRoot())

        logger.removeHandler(log_handler)
        log_handler.flush()
        log_buffer.flush()

        if log_buffer.getvalue():
            errors.append(output_api.PresubmitError(
                'Cannot parse WATCHLISTS file, please resolve.',
                log_buffer.getvalue().splitlines()))
    return errors


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    # We should figure out what license checks we actually want to use.
    license_header = r'.*'

    results = []
    results.extend(input_api.canned_checks.PanProjectChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS,
        maxlen=800, license_header=license_header))
    results.extend(input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
    results.extend(_CheckForNonBlinkVariantMojomIncludes(input_api, output_api))
    results.extend(_CheckForVersionControlConflicts(input_api, output_api))
    results.extend(_CheckPatchFiles(input_api, output_api))
    results.extend(_CheckTestExpectations(input_api, output_api))
    results.extend(_CheckChromiumPlatformMacros(input_api, output_api))
    results.extend(_CheckWatchlist(input_api, output_api))
    results.extend(_CheckFilePermissions(input_api, output_api))
    return results


def _CheckPatchFiles(input_api, output_api):
  problems = [f.LocalPath() for f in input_api.AffectedFiles()
      if f.LocalPath().endswith(('.orig', '.rej'))]
  if problems:
    return [output_api.PresubmitError(
        "Don't commit .rej and .orig files.", problems)]
  else:
    return []


def _CheckTestExpectations(input_api, output_api):
    local_paths = [f.LocalPath() for f in input_api.AffectedFiles()]
    if any('LayoutTests' in path for path in local_paths):
        lint_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
            'Tools', 'Scripts', 'lint-test-expectations')
        _, errs = input_api.subprocess.Popen(
            [input_api.python_executable, lint_path],
            stdout=input_api.subprocess.PIPE,
            stderr=input_api.subprocess.PIPE).communicate()
        if not errs:
            return [output_api.PresubmitError(
                "lint-test-expectations failed "
                "to produce output; check by hand. ")]
        if errs.strip() != 'Lint succeeded.':
            return [output_api.PresubmitError(errs)]
    return []


def _CheckStyle(input_api, output_api):
    # Files that follow Chromium's coding style do not include capital letters.
    re_chromium_style_file = re.compile(r'\b[a-z_]+\.(cc|h)$')
    style_checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        'Tools', 'Scripts', 'check-webkit-style')
    args = [input_api.python_executable, style_checker_path, '--diff-files']
    files = [input_api.os_path.join('..', '..', f.LocalPath())
             for f in input_api.AffectedFiles()
             # Filter out files that follow Chromium's coding style.
             if not re_chromium_style_file.search(f.LocalPath())]
    # Do not call check-webkit-style with empty affected file list if all
    # input_api.AffectedFiles got filtered.
    if not files:
        return []
    args += files
    results = []

    try:
        child = input_api.subprocess.Popen(args,
                                           stderr=input_api.subprocess.PIPE)
        _, stderrdata = child.communicate()
        if child.returncode != 0:
            results.append(output_api.PresubmitError(
                'check-webkit-style failed', [stderrdata]))
    except Exception as e:
        results.append(output_api.PresubmitNotifyResult(
            'Could not run check-webkit-style', [str(e)]))

    return results


def _CheckChromiumPlatformMacros(input_api, output_api, source_file_filter=None):
    """Ensures that Blink code uses WTF's platform macros instead of
    Chromium's. Using the latter has resulted in at least one subtle
    build breakage."""
    os_macro_re = input_api.re.compile(r'^\s*#(el)?if.*\bOS_')
    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not os_macro_re.search(x),
        input_api, source_file_filter)
    errors = ['Found use of Chromium OS_* macro in %s. '
        'Use WTF platform macros instead.' % violation for violation in errors]
    if errors:
        return [output_api.PresubmitPromptWarning('\n'.join(errors))]
    return []


def _CheckForPrintfDebugging(input_api, output_api):
    """Generally speaking, we'd prefer not to land patches that printf
    debug output."""
    printf_re = input_api.re.compile(r'^\s*(printf\(|fprintf\(stderr,)')
    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not printf_re.search(x),
        input_api, None)
    errors = ['  * %s' % violation for violation in errors]
    if errors:
        return [output_api.PresubmitPromptOrNotify(
                    'printf debugging is best debugging! That said, it might '
                    'be a good idea to drop the following occurances from '
                    'your patch before uploading:\n%s' % '\n'.join(errors))]
    return []


def _CheckForJSTest(input_api, output_api):
    """'js-test.js' is the past, 'testharness.js' is our glorious future"""
    jstest_re = input_api.re.compile(r'resources/js-test.js')

    def source_file_filter(path):
        return input_api.FilterSourceFile(path,
                                          white_list=[r'third_party/WebKit/LayoutTests/.*\.(html|js|php|pl|svg)$'])

    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not jstest_re.search(x), input_api, source_file_filter)
    errors = ['  * %s' % violation for violation in errors]
    if errors:
        return [output_api.PresubmitPromptOrNotify(
            '"resources/js-test.js" is deprecated; please write new layout '
            'tests using the assertions in "resources/testharness.js" '
            'instead, as these can be more easily upstreamed to Web Platform '
            'Tests for cross-vendor compatibility testing. If you\'re not '
            'already familiar with this framework, a tutorial is available at '
            'https://darobin.github.io/test-harness-tutorial/docs/using-testharness.html'
            '\n\n%s' % '\n'.join(errors))]
    return []

def _CheckForFailInFile(input_api, f):
    pattern = input_api.re.compile('^FAIL')
    errors = []
    for line_num, line in f.ChangedContents():
        if pattern.match(line):
            errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))
    return errors


def _CheckFilePermissions(input_api, output_api):
    """Check that all files have their permissions properly set."""
    if input_api.platform == 'win32':
        return []
    args = [input_api.python_executable,
            input_api.os_path.join(
                input_api.change.RepositoryRoot(),
                'tools/checkperms/checkperms.py'),
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


def _CheckForInvalidPreferenceError(input_api, output_api):
    pattern = input_api.re.compile('Invalid name for preference: (.+)')
    results = []

    for f in input_api.AffectedFiles():
        if not f.LocalPath().endswith('-expected.txt'):
            continue
        for line_num, line in f.ChangedContents():
            error = pattern.search(line)
            if error:
                results.append(output_api.PresubmitError('Found an invalid preference %s in expected result %s:%s' % (error.group(1), f, line_num)))
    return results


def _CheckForForbiddenNamespace(input_api, output_api):
    """Checks that Blink uses Chromium namespaces only in permitted code."""
    # This list is not exhaustive, but covers likely ones.
    chromium_namespaces = ["base", "cc", "content", "gfx", "net", "ui"]
    chromium_forbidden_classes = ["scoped_refptr"]
    chromium_allowed_classes = ["gfx::CubicBezier"]

    def source_file_filter(path):
        return input_api.FilterSourceFile(path,
                                          white_list=[r'third_party/WebKit/Source/.*\.(h|cpp)$'],
                                          black_list=[r'third_party/WebKit/Source/(platform|wtf|web)/'])

    comment_re = input_api.re.compile(r'^\s*//')
    result = []
    for namespace in chromium_namespaces:
        namespace_re = input_api.re.compile(r'\b{0}::([A-Za-z_][A-Za-z0-9_]*)'.format(input_api.re.escape(namespace)))

        def uses_namespace_outside_comments(line):
            if comment_re.search(line):
                return False
            re_result = namespace_re.search(line)
            if not re_result:
                return False
            parsed_class_name = namespace + "::" + re_result.group(1)
            return not (parsed_class_name in chromium_allowed_classes)

        errors = input_api.canned_checks._FindNewViolationsOfRule(lambda _, line: not uses_namespace_outside_comments(line),
                                                                  input_api, source_file_filter)
        if errors:
            result += [output_api.PresubmitError('Do not use Chromium class from namespace {} inside Blink core:\n{}'.format(namespace, '\n'.join(errors)))]
    for namespace in chromium_namespaces:
        namespace_re = input_api.re.compile(r'^\s*using namespace {0};|^\s*namespace {0} \{{'.format(input_api.re.escape(namespace)))
        uses_namespace_outside_comments = lambda line: namespace_re.search(line) and not comment_re.search(line)
        errors = input_api.canned_checks._FindNewViolationsOfRule(lambda _, line: not uses_namespace_outside_comments(line),
                                                                  input_api, source_file_filter)
        if errors:
            result += [output_api.PresubmitError('Do not use Chromium namespace {} inside Blink core:\n{}'.format(namespace, '\n'.join(errors)))]
    for class_name in chromium_forbidden_classes:
        class_re = input_api.re.compile(r'\b{0}\b'.format(input_api.re.escape(class_name)))
        uses_class_outside_comments = lambda line: class_re.search(line) and not comment_re.search(line)
        errors = input_api.canned_checks._FindNewViolationsOfRule(lambda _, line: not uses_class_outside_comments(line),
                                                                  input_api, source_file_filter)
        if errors:
            result += [output_api.PresubmitError('Do not use Chromium class {} inside Blink core:\n{}'.format(class_name, '\n'.join(errors)))]
    return result


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(_CheckStyle(input_api, output_api))
    results.extend(_CheckForPrintfDebugging(input_api, output_api))
    results.extend(_CheckForJSTest(input_api, output_api))
    results.extend(_CheckForInvalidPreferenceError(input_api, output_api))
    results.extend(_CheckForForbiddenNamespace(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(input_api.canned_checks.CheckTreeIsOpen(
        input_api, output_api,
        json_url='http://chromium-status.appspot.com/current?format=json'))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(
        input_api, output_api))
    return results


def _ArePaintOrCompositingDirectoriesModified(change):  # pylint: disable=C0103
    """Checks whether CL has changes to paint or compositing directories."""
    paint_or_compositing_paths = [
        os.path.join('third_party', 'WebKit', 'Source', 'platform', 'graphics',
                     'compositing'),
        os.path.join('third_party', 'WebKit', 'Source', 'platform', 'graphics',
                     'paint'),
        os.path.join('third_party', 'WebKit', 'Source', 'core', 'layout',
                     'compositing'),
        os.path.join('third_party', 'WebKit', 'Source', 'core', 'paint'),
    ]
    for affected_file in change.AffectedFiles():
        file_path = affected_file.LocalPath()
        if any(x in file_path for x in paint_or_compositing_paths):
            return True
    return False


def PostUploadHook(cl, change, output_api):  # pylint: disable=C0103
    """git cl upload will call this hook after the issue is created/modified.

    This hook adds extra try bots to the CL description in order to run slimming
    paint v2 tests in addition to the CQ try bots if the change contains paint
    or compositing changes (see: _ArePaintOrCompositingDirectoriesModified). For
    more information about slimming-paint-v2 tests see https://crbug.com/601275.
    """
    if not _ArePaintOrCompositingDirectoriesModified(change):
        return []

    rietveld_obj = cl.RpcServer()
    issue = cl.issue
    description = rietveld_obj.get_description(issue)
    if re.search(r'^CQ_INCLUDE_TRYBOTS=.*', description, re.M | re.I):
        return []

    bots = [
        'master.tryserver.chromium.linux:linux_layout_tests_slimming_paint_v2',
    ]

    results = []
    new_description = description
    new_description += '\nCQ_INCLUDE_TRYBOTS=%s' % ';'.join(bots)
    results.append(output_api.PresubmitNotifyResult(
        'Automatically added slimming-paint-v2 tests to run on CQ due to '
        'changes in paint or compositing directories.'))

    if new_description != description:
        rietveld_obj.update_description(issue, new_description)

    return results
