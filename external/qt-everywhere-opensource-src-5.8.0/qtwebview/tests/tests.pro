TEMPLATE = subdirs

# Currently the tests are adapted for the platforms
# that provide a native WebView implementation.
android|ios|winrt {
  SUBDIRS += auto
}
