# This is to test if quoted elements with spaces are treated as elements (and not splitted up due
# to the spaces.)
# It also tries to verify the behaviour of combining quoted and non-quoted elements with literals.
#

QUOTED = $$quote(variable with spaces)
VERSIONAB = "a.b"
VAB = $$split(VERSIONAB, ".")
V += $$VAB
V += $$QUOTED

# this is just to make p4 happy with no spaces in filename
SOURCES += $$member(V,0,1)
V2 = $$member(V,2)
V2S = $$split(V2, " ")
SOURCES += $$join(V2S,"_")
message($$SOURCES)
# SOURCES += [a, b, variable_with_spaces]

LIST = d e f
L2 = x/$$LIST/g.cpp
SOURCES += $$L2
# SOURCES += [x/d, e, f/g.cpp]

QUOTEDEXTRA = x/$$QUOTED/z
Q3 = $$split(QUOTEDEXTRA, " ")
SOURCES += $$Q3
# SOURCES += [x/variable, with, spaces/z]

win32: SOURCES += $$system(type files-cc.txt)
unix: SOURCES += $$system(cat files-cc.txt)

TRANSLATIONS = project.ts
