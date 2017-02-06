declare variable $externalVariableAsInt as xs:integer := xs:integer($externalVariable);
$externalVariable, $externalVariableAsInt + 3, <e>{$externalVariable}</e>, $externalVariable instance of xs:string
