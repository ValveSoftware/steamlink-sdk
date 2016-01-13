<xsl:stylesheet
    version="2.0"
    xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:param name="defaultedParam" select="'defParam'"/>
    <xsl:param name="overridedDefaultedParam" select="'DOESNOTAPPEAR'"/>
    <xsl:param name="implicitlyRequiredValue" as="xs:string"/>

    <xsl:template name="main">
        <xsl:sequence select="$defaultedParam,
                              $overridedDefaultedParam,
                              $implicitlyRequiredValue"/>
    </xsl:template>

</xsl:stylesheet>
