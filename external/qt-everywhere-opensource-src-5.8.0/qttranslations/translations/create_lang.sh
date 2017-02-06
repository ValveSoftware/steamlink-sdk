#!/bin/bash

pushd ../../build/qttranslations/translations
make ts-untranslated
popd

for lang in bg cs da el es fi fr hu it ja ko nb nl pl pt ro ru sv th tr zh_CN zh_TW
do for file in *untranslated*
   do new=$(echo $file | sed "s,untranslated,$lang,")
      if [ -f $new ]; then : ; else cp -v $file $new; fi
   done
done
rm -f *untranslated*
