for $file in tokenize($files, codepoints-to-string(10))
    let $fresh := doc($file)/TS/context/message[not (translation/@type = ('obsolete', 'vanished'))]
    return if (count($fresh) gt 0)
           then concat($file, ":", count($fresh/translation[not (@type = 'unfinished')]) * 100 idiv count($fresh))
           else concat($file, ":n/a")
