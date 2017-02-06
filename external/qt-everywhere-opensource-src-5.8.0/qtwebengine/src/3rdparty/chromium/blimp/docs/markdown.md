# How to use Markdown

Markdown is a lightweight markup language which can be easily converted to
HTML.

## Gitiles reference guide

For the full reference guide for the version of Markdown used to render Chromium
pages, see the [reference for Gitiles Markdown][gtref].

## Local preview of changes

To see how the pages you edit will look, run the following command from your
Chromium checkout (`src/` directory) to start up a server on
http://localhost:8080/:

```bash
python tools/md_browser/md_browser.py
```

You can then navigate to http://localhost:8080/blimp/README.md
in your browser to see the result locally. Just refresh the page after saving
a `*.md` file to see an updated result.

[gtref]: https://gerrit.googlesource.com/gitiles/+/master/Documentation/markdown.md
