# Vulcanizing Material Design downloads

`vulcanize` is an npm module used to combine resources.  In order to make the
Material Design downloads page sufficiently fast, we run vulcanize on the source
files to combine them and reduce blocking load/parse time.

## Required software

Vulcanization currently requires:

- node.js: >= v4.4.2 (can be found with `node --version`)
- npm: >= 1.3.10 (can be found with `npm --version`)
- vulcanize: 1.14.8 (can be found with `vulcanize --version`)
- crisper: 2.0.1 (can be found with `npm list -g crisper`)

## Installing required software

For instructions on installing node and npm, see
[here](https://docs.npmjs.com/getting-started/installing-node).

We recommend telling npm where to store downloaded modules:

```bash
$ npm config set -g prefix "$HOME/node_modules"
```

Then install `crisper` and `vulcanize` like this:

```bash
$ npm install -g crisper vulcanize
```

Ultimately, all that is required to run this script is that `crisper` and
`vulcanize` are on your `$PATH`.

## Combining resources with vulcanize

To combine all the CSS/HTML/JS for the downloads page to make it production
fast, you can run the commands:

```bash
$ chrome/browser/resources/md_downloads/vulcanize.py  # from src/
```

This should overwrite the following files:

- chrome/browser/resources/md_downloads/
 - vulcanized.html (all <link rel=import> and stylesheets inlined)
 - crisper.js (all JavaScript, extracted from vulcanized.html)

## Testing downloads without vulcanizing

Build with "use_vulcanize=0" in your GYP_DEFINES to build downloads without
vulcanizing.
