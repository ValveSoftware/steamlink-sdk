make ts-untranslated
for file in *untranslated*
do new=$(echo $file | sed "s,untranslated,$1,")
   if [ -f $new ]; then rm $file; else mv -v $file $new; fi
done
