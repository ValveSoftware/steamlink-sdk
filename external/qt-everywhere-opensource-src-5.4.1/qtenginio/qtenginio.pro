# iOS does not require OpenSSL and always passes
!ios:requires(contains(QT_CONFIG, ssl) | contains(QT_CONFIG, openssl) | contains(QT_CONFIG, openssl-linked))

lessThan(QT_MAJOR_VERSION, 5): error("The Enginio Qt library only supports Qt 5.")
load(configure)
load(qt_parts)

QMAKE_DOCS = $$PWD/doc/qtenginiooverview.qdocconf
load(qt_docs)
OTHER_FILES += \
    doc/enginio_overview.qdoc \
    doc/qtenginiooverview.qdocconf

contains(CONFIG, coverage) {
  gcc*: {
    message("Enabling test coverage instrumentization")
    coverage.commands = lcov -t "Enginio" -o result1.info -c -d . \
                    && lcov -e result1.info *enginio* -o result2.info \
                    && lcov -r result2.info moc*   -o result.info \
                    && genhtml -o results result.info
    coverage.target = check_coverage
    coverage.depends = check
    QMAKE_EXTRA_TARGETS += coverage
  } else {
    warning("Test coverage is supported only through gcc")
  }
}

contains(DEFINES, ENGINIO_VALGRIND_DEBUG) {
  warning("ENGINIO_VALGRIND_DEBUG is enabled, ssl cipher will be hardcoded, DO NOT USE IT IN PRODUCTION")
}
