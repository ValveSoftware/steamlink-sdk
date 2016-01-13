{
  'target_defaults': {
    # patterns used to exclude chromium files from the build when we have a drop-in replacement
    'sources/': [
      # We are using gl_context_qt.cc instead.
      ['exclude', 'gl_context_ozone.cc$'],
    ],
  },
}
