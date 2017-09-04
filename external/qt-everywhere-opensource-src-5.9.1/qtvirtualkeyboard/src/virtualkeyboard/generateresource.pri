defineReplace(generate_resource) {
    GENERATED_FILE = $$OUT_PWD/$$1
    INCLUDED_FILES = $$2
    BASE_PREFIX = $$3
    GENERATED_CONTENT = \
        "<RCC>"

    RESOURCE_PREFIX = ""
    for (FILE, INCLUDED_FILES) {
        RELATIVE_PATH = $$relative_path($$absolute_path($$FILE), $$_PRO_FILE_PWD_)
        TEST_PREFIX = $$BASE_PREFIX/$$dirname(RELATIVE_PATH)
        !equals(TEST_PREFIX, $$RESOURCE_PREFIX) {
            !isEmpty(RESOURCE_PREFIX): GENERATED_CONTENT += "    </qresource>"
            RESOURCE_PREFIX = $$TEST_PREFIX
            GENERATED_CONTENT += "    <qresource prefix=\"$$RESOURCE_PREFIX\">"
        }
        ABSOLUTE_PATH = $$absolute_path($$FILE)
        ALIAS_NAME = $$basename(FILE)
        GENERATED_CONTENT += "        <file alias=\"$$ALIAS_NAME\">$$ABSOLUTE_PATH</file>"
    }
    !isEmpty(RESOURCE_PREFIX): GENERATED_CONTENT += "    </qresource>"

    GENERATED_CONTENT += \
        "</RCC>"
    write_file($$GENERATED_FILE, GENERATED_CONTENT)|error("Failed to write resource file!")

    return($$GENERATED_FILE)
}
