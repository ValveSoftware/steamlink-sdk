(: fromQTime is intentionally not here, since
   we don't support it. :)

string-join(for $i in (
                       $fromFloat,
                       $fromBool,
                       $fromDouble,
                       $fromIntLiteral,
                       $fromLongLong,
                       $fromQByteArray,
                       $fromQChar,
                       $fromQDate,
                       $fromQDateTime,
                       $fromQString,
                       $fromQUrl,
                       $fromUInt,
                       $fromULongLong,
                       $fromBool instance of xs:boolean,
                       $fromDouble instance of xs:double,
                       $fromFloat instance of xs:double,
                       $fromIntLiteral instance of xs:integer,
                       $fromLongLong instance of xs:integer,
                       $fromQByteArray instance of xs:base64Binary,
                       $fromQChar instance of xs:string,
                       $fromQDate instance of xs:date,
                       $fromQDateTime instance of xs:dateTime,
                       $fromQString instance of xs:string,
                       $fromQUrl instance of xs:string,
                       $fromUInt instance of xs:integer,
                       $fromULongLong instance of xs:unsignedLong)
              return string($i),
             " ")

