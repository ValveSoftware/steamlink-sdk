#!/bin/sh

PATCHFILE=localization.patch
: >$PATCHFILE

for i in qtbase_el.ts qtbase_nb.ts qtbase_nl.ts qtbase_pt.ts qtbase_ro.ts qtbase_sv.ts qtbase_th.ts qtbase_tr.ts qtbase_zh_CN.ts qtbase_zh_TW.ts
do diff -u qtbase_untranslated.ts $i >>$PATCHFILE
done

echo "If you are done, remove qtbase_untranslated.ts"
