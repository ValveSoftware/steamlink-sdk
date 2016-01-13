<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:template match="/
        <out>
            <xsl:copy-of select="document('bug42.xml', /)"/>
        </out>
    </xsl:template>

</xsl:stylesheet>
