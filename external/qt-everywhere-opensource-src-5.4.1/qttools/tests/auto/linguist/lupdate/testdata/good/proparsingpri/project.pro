include(win/win.pri)
more = mac unix
for(dir, more): \
    include($$dir/$${dir}.pri)
include (common/common.pri)             # Important: keep the space before the '('
include(relativity/relativity.pri)

message($$SOURCES)

TRANSLATIONS = project.ts
