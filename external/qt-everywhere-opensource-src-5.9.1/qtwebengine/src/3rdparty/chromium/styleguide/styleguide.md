# Chromium coding style

## Main style guides

  * [Chromium C++ style guide](c++/c++.md)
  * [Google Objective-C style guide](https://google.github.io/styleguide/objcguide.xml)
  * [Java style guide for Android](https://sites.google.com/a/chromium.org/dev/developers/coding-style/java)
  * [GN style guide](../tools/gn/docs/style_guide.md) for build files

Chromium also uses these languages to a lesser degree:

  * [Kernel C style](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/CodingStyle?id=refs/heads/master) for ChromiumOS firmware.
  * [IDL](https://sites.google.com/a/chromium.org/dev/blink/webidl#TOC-Style)
  * [Jinja style guide](https://sites.google.com/a/chromium.org/dev/developers/jinja#TOC-Style) for [Jinja](https://sites.google.com/a/chromium.org/dev/developers/jinja) templates.

## Python

Python code should follow [PEP-8](https://www.python.org/dev/peps/pep-0008/),
except:

  * Use two-space indentation instead of four-space indentation.
  * Use `CamelCase()` method and function names instead of `unix_hacker_style()` names.

(The rationale for these is mostly legacy: the code was originally written
following Google's internal style guideline, the cost of updating all of the
code to PEP-8 compliance was not small, and consistency was seen to be a
greater virtue than compliance.)

[Depot tools](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html)
contains a local copy of pylint, appropriately configured.

Note that asserts are of limited use, and should not be used for validating
input – throw an exception instead. Asserts can be used for validating program
logic, especially use of interfaces or invariants (e.g., asserting that a
function is only called with dictionaries that contain a certain key). [See
Using Assertions
Effectively](https://wiki.python.org/moin/UsingAssertionsEffectively).

See also the [Chromium OS Python Style
Guidelines](https://sites.google.com/a/chromium.org/dev/chromium-os/python-style-guidelines).

## Web languages (JavaScript, HTML, CSS)

When working on Web-based UI features, consult the [Web Development Style Guide](https://sites.google.com/a/chromium.org/dev/developers/web-development-style-guide) for the Chromium conventions used in JS/CSS/HTML files.

Internal uses of web languages, notably "layout" tests, should preferably follow these style guides, but it is not enforced.
