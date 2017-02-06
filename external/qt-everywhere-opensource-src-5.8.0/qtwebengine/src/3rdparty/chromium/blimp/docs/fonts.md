# Updating Blimp Fonts

1.  Clone the git-repositories listed in
    `//third_party/blimp_fonts/README.chromium`, and roll forward to the commit
    you want.
1.  Copy the necessary files to `//third_party/blimp_fonts/font_bundle`.
1.  Verify that the `fonts.xml` file include the correct fonts.
1.  Verify that the `LICENSE` file is still up to date and lists all relevant
    licenses and which fonts use which license.
1.  Update the `//third_party/blimp_fonts` target to include all the
    current fonts and their license files.
1.  Update the engine dependencies using
    `//blimp/tools/generate-engine-manifest.py`. This step is documented in
    `//blimp/docs/container.md`.
1.  Run the `upload_to_google_storage.py` (from depot_tools) script to upload
    the files. You must do this in the `//third_party/blimp_fonts` directory.
    To do this, execute:

    ```bash
    upload_to_google_storage.py --archive -b chromium-fonts font_bundle
    ```

1.  Add all the `font_bundle.tar.gz.sha1` file to the chromium src repository,
    by executing the following command:

    ```bash
    git add ./third_party/blimp_fonts/font_bundle.tar.gz.sha1
    ```

1.  Commit and upload the change for review:

    ```bash
    git commit
    git cl upload
    ```
