# Vulcanizing Chrome Polymer UIs

`vulcanize` is an npm module used to combine resources.  In order to make the
Material Design downloads and history pages sufficiently fast, we run vulcanize
on the source files to combine them and reduce blocking load/parse time.

## Required software

Vulcanization currently requires:

- node.js: >= v4.4.2 (can be found with `node --version`)
- npm: >= 1.3.10 (can be found with `npm --version`)
- vulcanize: 1.14.8 (can be found with `vulcanize --version`)
- crisper: 2.0.1 (can be found with `npm list -g crisper`)
- uglifyjs: 2.4.10 (can be found with `uglifyjs --version`)
- polymer-css-build: 0.0.6 (can be found with `npm list -g polymer-css-build`)

## Installing required software

For instructions on installing node and npm, see
[here](https://docs.npmjs.com/getting-started/installing-node).

We recommend telling npm where to store downloaded modules:

```bash
$ npm config set -g prefix "$HOME/node_modules"
```

Then install the required modules:

```bash
$ npm install -g crisper vulcanize uglifyjs polymer-css-build
```

Ultimately, all that is required to run this script is that the node binaries
listed above are on your $PATH.

## Combining resources with vulcanize

To combine all the CSS/HTML/JS for all pages which use vulcanize, making them
production fast, you can run the command:

```bash
$ chrome/browser/resources/vulcanize.py  # from src/
```

This should overwrite the following files:

- chrome/browser/resources/md_downloads/
 - vulcanized.html (all <link rel=import> and stylesheets inlined)
 - crisper.js (all JavaScript, extracted from vulcanized.html)
- chrome/browser/resources/md_history/
 - app.vulcanized.html
 - app.crisper.js

## Testing downloads without vulcanizing

Build with `use_vulcanize = false` in your gn args to build without vulcanizing.
