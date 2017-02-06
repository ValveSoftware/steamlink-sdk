# Updating clang

1.  Sync your Chromium tree to the latest revision to pick up any plugin
    changes
1.  Run `python tools/clang/scripts/upload_revision.py --clang_revision=NNNN`
    with the target LLVM SVN revision number
1.  If the clang upload trybots succeed, run the goma package update script to
    push these packages to goma. If you do not have the necessary credentials to
    do the upload, ask clang@chromium.org to find someone who does
1.  Run an exhaustive set of try jobs to test the new compiler:
```
    git cl try &&
    git cl try -m tryserver.chromium.mac -b mac_chromium_asan_rel_ng \
      -b mac_chromium_gyp_dbg &&
    git cl try -m tryserver.chromium.linux -b linux_chromium_chromeos_dbg_ng \
      -b linux_chromium_chromeos_asan_rel_ng -b linux_chromium_msan_rel_ng &&
    git cl try -m tryserver.blink -b linux_blink_rel
```
1.  Commit roll CL from the first step
1.  The bots will now pull the prebuilt binary, and goma will have a matching
    binary, too.
