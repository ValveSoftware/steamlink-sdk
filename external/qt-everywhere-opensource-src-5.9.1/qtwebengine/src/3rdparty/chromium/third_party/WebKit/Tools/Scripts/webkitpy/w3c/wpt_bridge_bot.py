# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# DO NOT RUN THIS CODE. If this runs on multiple machines, we'll see
# inconsistent behavior, and will mess up both of Chromium project and
# web-platform-tests project.
# We checked in this code to share a part of the code, and will remove this
# later.

# * Setting for Gerrit access
#   1. Create a service account and get a JSON file including a private key.
#   2. Put the JSON file to this directory as "wpt-chromium-bridge.json".
#   2. Configure gerrit server so that the service account can
#     - Add a comment
#     - Update Verified label
#     - Update Commit-Queue label on behalf of someone.
#     - Abandon reviews
#
# * Setting for GitHub access via git
#   Register ssh host "github-bot" to access github.com with an account who can
#   push to the master WPT repository.
#  e.g.
#   1. Generate a ssh key pair as ~/.ssh/ghbotaccount_id_rsa* without passphrase
#   2. ~/.ssh/config
#     Host github-bot
#         Hostname github.com
#         User git
#         IdentityFile ~/.ssh/ghbotaccount_id_rsa
#   3. Register ghbotaccount_id_rsa.pub to a pushable account on github.com.
#   4. Access github-bot once via git/ssh manually to register a host key of
#     github.com.
#
# * Setting for GitHub access via REST API
#   1. Obtain a personal access token.  See https://help.github.com/articles/creating-an-access-token-for-command-line-use/
#   2. Put your GitHub account name and the access token to ./github-rest-account.json like
#     { "name": "tkent-google", "token": "ABCDEF...."}

import argparse
import base64
import httplib2
import json
import math
import os
import re
import subprocess
import tempfile
import time

from oauth2client.service_account import ServiceAccountCredentials

# We should have a checkout on WORKING_COPY_DIR/web-platform-tests.
# We check it out with a pushable account.
# We may make unique tmp dir on every startup if this code runs for a long time.
WORKING_COPY_DIR = "~/src"
WPT_DIR = WORKING_COPY_DIR + "/web-platform-tests"

PENDING_MERGE_FILE = "./pending_merge.json"

STOP_FILE = "./STOP"

GITHUB_REST_ACCOUNT_FILE = "./github-rest-account.json"

# The list of people who has WPT push privilege and is a chromium committer.
# https://github.com/orgs/w3c/teams/web-platform-testing
OWNER_LIST = [
    "dominicc@chromium.org",
    "foolip@chromium.org",
    "hayato@chromium.org",
    "jsbell@chromium.org",
    "kochi@chromium.org",
    "kojii@chromium.org",
    "mkwst@chromium.org",
    "rbyers@chromium.org",
    "tkent@chromium.org"
]


def create_owner_list(indent=''):
    result = ''
    for email in OWNER_LIST:
        result += indent + email + "\n"
    return result


def is_owner_id(email):
    for entry in OWNER_LIST:
        if entry == email:
            return True
    return False


def run_command_in_wpt_dir(argv):
    return subprocess.call(argv, cwd=os.path.expanduser(WPT_DIR))


def ensure_clean_checkout():
    if not os.access(os.path.expanduser(WPT_DIR), os.R_OK):
        print "ensure_clean_checkout: Full clone"
        work_dir = os.path.expanduser(WORKING_COPY_DIR)
        if not os.access(work_dir, os.R_OK):
            os.mkdir(work_dir)
        subprocess.call(["git", "clone", "--recursive", "github-bot:w3c/web-platform-tests.git"], cwd=work_dir)
        return
    print "EnsureCleanChckout: update"
    run_command_in_wpt_dir(["git", "checkout", "-f", "master"])
    run_command_in_wpt_dir(["git", "reset", "--hard", "HEAD"])
    run_command_in_wpt_dir(["git", "pull"])
    run_command_in_wpt_dir(["git", "submodule", "update", "--recursive"])


# ================================================================
# https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html

def gerrit_create_protocol():
    #  https://gerrit-internal.git.corp.google.com/docs/+/master/users/from-gce.md
    scopes = ['https://www.googleapis.com/auth/gerritcodereview']
    credentials = ServiceAccountCredentials.from_json_keyfile_name(
        './wpt-chromium-bridge.json', scopes=scopes)
    return credentials.authorize(httplib2.Http())


def gerrit_get_json(endpoint, method, request_json):
    http = gerrit_create_protocol()
    request_body = None
    request_headers = {}
    if request_json is not None:
        request_body = json.JSONEncoder().encode(request_json)
        request_headers["Content-Type"] = "application/json"
    (headers, content) = http.request("https://chromium-review.googlesource.com/a/%s" % endpoint,
                                      method,
                                      body=request_body, headers=request_headers)
    if headers["status"] != "200":
        print content
        raise Exception("An Gerrit error; status=%s" % headers["status"])
    if content.startswith(")]}'\n"):
        return json.loads(content[5:])
    raise Exception("An Gerrit error; content=%s" % content)


def gerrit_query_open_changes():
    # can we limit branch=master by query?
    return gerrit_get_json("changes/?q=project:external/w3c/web-platform-tests+status:open&o=LABELS&o=DETAILED_ACCOUNTS",
                           "GET", None)


def gerrit_add_review(change_id, message, labels=None, on_behalf_of=None):
    request = {"message": "=== WPT bridge bot ===\n" + message}
    if labels is not None:
        request["labels"] = labels
    if on_behalf_of is not None:
        request["on_behalf_of"] = on_behalf_of

    print request
    gerrit_get_json("changes/%d/revisions/current/review" % change_id, "POST", request)


def gerrit_close(change_id, message):
    request = {"message": "=== WPT bridge bot ===\n" + message}
    print request
    gerrit_get_json("changes/%d/abandon" % change_id, "POST", request)


def gerrit_fetch_patch(change_id):
    print "gerrit_fetch_patch:"
    http = gerrit_create_protocol()
    (headers, content) = http.request("https://chromium-review.googlesource.com/a/changes/%s/revisions/current/patch" % change_id,
                                      "GET")
    if headers["status"] != "200":
        raise Exception("An Gerrit error; status=%s" % headers["status"])
    return base64.b64decode(content)


# ================================================================

def github_auth_token():
    with open(GITHUB_REST_ACCOUNT_FILE) as fh:
        json_obj = json.loads(fh.read())
        return base64.encodestring(json_obj["name"] + ':' + json_obj["token"])


def github_create_pr(branch_name, desc_title, gerrit_review_url):
    # https://developer.github.com/v3/pulls/#create-a-pull-request
    conn = httplib2.Http()
    headers = {
        "Accept": "application/vnd.github.v3+json",
        "Authorization": "Basic " + github_auth_token()
    }
    body = {
        "title": desc_title,
        "body": "Merge from %s\n\nA bot created this PR, and will merge this in some minutes.\n" +
                "People don't need to review this.\n" % gerrit_review_url,
        "head": branch_name,
        "base": "master"
    }
    resp, content = conn.request("https://api.github.com/repos/w3c/web-platform-tests/pulls",
                                 "POST", body=json.JSONEncoder().encode(body), headers=headers)
    print "GitHub response:"
    print content
    if resp["status"] != "201":
        return None
    return json.loads(content)


def github_merge(pr_number, desc_title, desc_body):
    # https://developer.github.com/v3/pulls/#merge-a-pull-request-merge-button
    headers = {
        # commit_title and merge_method isn't avaiable in v3 API.
        "Accept": "application/vnd.github.polaris-preview+json",
        "Authorization": "Basic " + github_auth_token()
    }
    body = {
        "commit_title": desc_title,
        "commit_message": desc_body,
        "merge_method": "squash"
    }
    conn = httplib2.Http()
    resp, content = conn.request("https://api.github.com/repos/w3c/web-platform-tests/pulls/%d/merge" % pr_number,
                                 "PUT", body=json.JSONEncoder().encode(body), headers=headers)
    print resp
    print content
    status = resp["status"]
    if status == "200" or status == "405" or status == "409":
        return json.loads(content)
    return None


# ================================================================

def apply_patch(patch):
    temp = tempfile.NamedTemporaryFile(delete=False)
    print "apply_patch: tmp=" + temp.name
    temp.write(patch)
    temp.close()
    retcode = run_command_in_wpt_dir(["git", "apply", "--index", temp.name])
    os.remove(temp.name)
    return retcode == 0


def do_chromium_presubmit(patch):
    print "do_chromium_presubmit"
    # TODO(tkent): Should check lines starting with '+'.
    if re.search(r"\W(internals|testRunner|eventSender)\.", patch):
        return False
    return True


def try_patch(change_id, patch, branch_name):
    run_command_in_wpt_dir(["git", "checkout", "-b", branch_name])
    # ChangeInfo.mergeable indicates whether we can apply the patch.  However we
    # actually need to apply the patch because we run |lint|.
    patch_result = apply_patch(patch)
    lint_result = None
    presubmit_result = None
    if patch_result:
        # TODO(tkent): Show lint output.
        _, _, _, files = extract_description(patch, change_id)
        print "try_patch: run lint"
        command = ["python", "lint"]
        command += files
        lint_result = 0 == run_command_in_wpt_dir(command)
        presubmit_result = do_chromium_presubmit(patch)
    return (patch_result, lint_result, presubmit_result)


def clean_branch(branch_name):
    run_command_in_wpt_dir(["find", ".", "-name", "*.rej", "-delete"])
    run_command_in_wpt_dir(["git", "reset", "--hard", "HEAD"])
    run_command_in_wpt_dir(["git", "checkout", "master"])
    run_command_in_wpt_dir(["git", "branch", "-D", branch_name])


def verify_change(change_id, dry_run=False):
    print ">>> Verifying %d ================================================================" % change_id
    ensure_clean_checkout()
    patch = gerrit_fetch_patch(change_id)
    branch_name = "chromium-try-%d-%.0f" % (change_id, time.time())
    (result, lint_result, presubmit_result) = try_patch(change_id, patch, branch_name)
    clean_branch(branch_name)
    lint_result_string = "N/A"
    if lint_result:
        lint_result_string = "OK"
    elif lint_result is False:
        lint_result_string = "NG"
    presubmit_result_string = "N/A"
    if presubmit_result:
        presubmit_result_string = "OK"
    elif presubmit_result is False:
        presubmit_result_string = "NG"
    status = "  Apply patch: %s\n  WPT lint: %s\n  Chromium-specific check: %s\n" % \
             ("OK" if result else "NG", lint_result_string, presubmit_result_string)
    message = None
    labels = None
    # TODO(tkent): Should show commit ID.
    if result and lint_result and presubmit_result:
        message = "Found no problems in this change:\n" + status + \
                  "\nThis change needs Code-Review+2 from one of the following people if you are not one of them:\n" + \
                  create_owner_list(indent='  ')
        labels = {"Verified": 1}
    else:
        message = "Found this change had problems:\n%s" % status
        labels = {"Verified": -1}
    print "verify_change:"
    print message, labels
    if not dry_run:
        gerrit_add_review(change_id, message, labels)


def find_and_verify_changes(changes):
    for change_info in changes:
        if len(change_info["labels"]["Verified"]) == 0:
            try:
                verify_change(change_info["_number"], dry_run=False)
            except Exception as ex:
                print ex


def split_description(cl_description):
    linebreak = cl_description.find("\n")
    if linebreak == -1:
        return cl_description, ""
    return cl_description[0:linebreak], cl_description[linebreak + 1:]


def extract_description(patch, change_id):
    author = re.search(r"^From: (.*)$", patch, re.MULTILINE).group(1)
    match = re.search(r"^Subject: \[PATCH\] (.*)^(Change-Id: \w+\n)", patch, re.DOTALL | re.MULTILINE)
    review_url = "https://chromium-review.googlesource.com/%d" % change_id

    cl_description = match.group(1) + ("Author: %s\n" % author) + ("Review: %s\n\n" % review_url) + match.group(2)
    title, body = split_description(cl_description)

    files = []
    for match in re.finditer(r"^\+\+\+ [^/]+/(.*?)$", patch, re.MULTILINE):
        files.append(match.group(1))

    return title, body, review_url, files


def enqueue_merge_request(change_id, cq_setter, pr_number, branch_name, timestamp):
    merge_record = {"change_id": change_id,
                    "cq_setter": cq_setter,
                    "pr_number": pr_number,
                    "branch_name": branch_name,
                    "pr_created": timestamp}
    queue = load_merge_request_queue()
    queue.append(merge_record)
    save_merge_request_queue(queue)


def load_merge_request_queue():
    if os.access(PENDING_MERGE_FILE, os.R_OK):
        with open(PENDING_MERGE_FILE) as fh:
            return json.loads(fh.read())
    return []


def save_merge_request_queue(queue):
    if os.access(PENDING_MERGE_FILE, os.R_OK):
        backup = PENDING_MERGE_FILE + "~"
        if os.access(backup, os.R_OK):
            os.remove(backup)
        os.rename(PENDING_MERGE_FILE, backup)
    if len(queue) > 0:
        with open(PENDING_MERGE_FILE, "wb") as fh:
            fh.write(json.dumps(queue))


def commit_change(change_info, dry_run=False):
    change_id = change_info["_number"]
    print ">>> Committing %d ================================================================" % change_id

    cq_setter = change_info["labels"]["Commit-Queue"]["recommended"]["_account_id"]
    # Verify approver
    if "approved" not in change_info["labels"]["Code-Review"]:
        gerrit_add_review(change_id, "This isn't approved yet.\n",
                          labels={"Commit-Queue": 0}, on_behalf_of=cq_setter)
        return
    if not is_owner_id(change_info["labels"]["Code-Review"]["approved"]["email"]) and \
       not is_owner_id(change_info["owner"]["email"]):
        gerrit_add_review(change_id, "This isn't approved by an appropriate people.\n",
                          labels={"Commit-Queue": 0}, on_behalf_of=cq_setter)
        return

    print ">> Passed owner check"
    print change_info
    ensure_clean_checkout()
    patch = gerrit_fetch_patch(change_id)
    branch_name = "chromium-review-%d-%.0f" % (change_id, math.floor(time.time()))
    (result, lint_result, presubmit_result) = try_patch(change_id, patch, branch_name)
    if result and lint_result and presubmit_result:
        desc_title, desc_body, review_url, _ = extract_description(patch, change_id)
        cl_description = desc_title + "\n" + desc_body
        print "Change Description:\n" + cl_description
        if dry_run:
            print ">> dry_run: Aborted. branch_name='%s' title='%s' body='%s'\n" % (branch_name, desc_title, desc_body)
            return
        run_command_in_wpt_dir(["git", "commit", "-a", "-m", cl_description])
        run_command_in_wpt_dir(["git", "push", "origin", branch_name])

        # Make a PR on GitHub.
        pr_result = github_create_pr(branch_name, desc_title, review_url)
        if pr_result is None or "html_url" not in pr_result or "number" not in pr_result:
            # TODO(tkent): The following command fails if Gerrit is offline, and
            # the bot will try to make a new PR again.
            gerrit_add_review(change_id, "Failed to create a pull request.\n",
                              labels={"Commit-Queue": 0}, on_behalf_of=cq_setter)
            clean_branch(branch_name)
            run_command_in_wpt_dir(["git", "push", "--delete", "origin", branch_name])
            return
        clean_branch(branch_name)

        # Merge immediately didn't work.  We need to wait for at least three minutes.
        enqueue_merge_request(change_id, cq_setter, pr_result["number"], branch_name, math.floor(time.time()))
        gerrit_add_review(change_id, "Created a pull request: " + pr_result["html_url"] + "\n")
    else:
        # TODO(tkent): write a message about the failure.
        gerrit_add_review(change_id, "Verification failure.\n",
                          labels={"Commit-Queue": 0}, on_behalf_of=cq_setter)
    return


def find_and_commit_changes(changes):
    for change_info in changes:
        if "recommended" in change_info["labels"]["Commit-Queue"]:
            try:
                commit_change(change_info, dry_run=False)
            except Exception as ex:
                print ex


def merge(request):
    pr_number = request["pr_number"]
    change_id = request["change_id"]
    print "===> Merge PR#%d for gerrit/%d" % (pr_number, change_id)

    try:
        patch = gerrit_fetch_patch(change_id)
        desc_title, desc_body, _, _ = extract_description(patch, change_id)
        merge_result = github_merge(pr_number, desc_title, desc_body)
        if merge_result is None:
            gerrit_add_review(change_id, "Failed to merge the pull request.\n",
                              labels={"Commit-Queue": 0}, on_behalf_of=request["cq_setter"])
            # Should close the PR?
        elif "merged" in merge_result:
            # TODO(tkent): The following command fails if Gerrit is offline, and
            # the bot will try to make a new PR again.
            gerrit_close(change_id, "Merged the PR: https://github.com/w3c/web-platform-tests/commit/%s\n" +
                         "Closing this review.\n" % merge_result["sha"])
        elif re.search(r"pending", merge_result["message"]):
            # Still need to wait.
            return False
        else:
            gerrit_add_review(change_id, "Failed to merge the pull request: %s\n" % merge_result["message"],
                              labels={"Commit-Queue": 0}, on_behalf_of=request["cq_setter"])
            # TODO(tkent): Should close the PR?
        run_command_in_wpt_dir(["git", "push", "--delete", "origin", request["branch_name"]])
    except Exception as ex:
        print ex
        return False
    return True


def run_once_command(_):
    queue = load_merge_request_queue()
    # print "Loaded queue: ", queue
    cloned_queue = queue[:]
    for request in cloned_queue:
        if merge(request):
            queue.remove(request)
    save_merge_request_queue(queue)

    changes = gerrit_query_open_changes()
    # Remove changes in |queue|.
    cloned_changes = changes[:]
    for change_info in cloned_changes:
        if not change_info["branch"].endswith("master"):
            changes.remove(change_info)
            continue
        for request in queue:
            if change_info["_number"] == request["change_id"] or \
               not change_info["branch"].endswith("master"):
                changes.remove(change_info)
                break
    print "==> Target changes: at %s" % time.asctime()
    for change_info in changes:
        if "approved" in change_info["labels"]["Verified"]:
            change_info["labels"]["Verified"]["approved"]["avatars"] = "***DELETED***"
        change_info["owner"]["avatars"] = "***DELETED***"
        print change_info
    find_and_verify_changes(changes)
    find_and_commit_changes(changes)


def run_loop_command(options):
    while not os.access(STOP_FILE, os.R_OK):
        run_once_command(options)
        print "==> Sleeping..."
        time.sleep(5 * 60)
    print "==> Found %s file.  Aborted." % STOP_FILE


def fetch_patch_command(options):
    patch = gerrit_fetch_patch(options.change_id)
    print patch
    desc_title, desc_body, _, files = extract_description(patch, options.change_id)
    print ">> patch command: title='%s' body='%s'\nFiles:\n%s" % (desc_title, desc_body, "\n".join(files))


def gerrit_comment_command(options):
    labels = {}
    if options.clear_cq:
        labels["Submit"] = 1
    gerrit_add_review(options.change_id, options.message, labels=labels)


def gerrit_get_command(options):
    print gerrit_get_json(options.endpoint, "GET", None)


def main():
    parser = argparse.ArgumentParser(description='WPT bridge bot')
    sub_parsers = parser.add_subparsers()

    sub_parser = sub_parsers.add_parser('once')
    sub_parser.set_defaults(func=run_once_command)

    sub_parser = sub_parsers.add_parser('loop')
    sub_parser.set_defaults(func=run_loop_command)

    sub_parser = sub_parsers.add_parser('patch')
    sub_parser.add_argument('change_id', type=int)
    sub_parser.set_defaults(func=fetch_patch_command)

    sub_parser = sub_parsers.add_parser('gerrit-comment')
    sub_parser.add_argument('change_id', type=int)
    sub_parser.add_argument('message', type=str)
    sub_parser.add_argument('--clear-cq', action='store_true')
    sub_parser.set_defaults(func=gerrit_comment_command)

    sub_parser = sub_parsers.add_parser('gerrit-get')
    sub_parser.add_argument('endpoint', type=str)
    sub_parser.set_defaults(func=gerrit_get_command)

    options = parser.parse_args()
    options.func(options)

if __name__ == '__main__':
    main()
