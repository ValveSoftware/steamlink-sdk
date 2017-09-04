<xsl:stylesheet version="2.0"
                xmlns:ts="http://www.w3.org/XML/2004/xml-schema-test-suite/"
                xmlns:xlink="http://www.w3.org/1999/xlink"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:template match="ts:testSetRef">
        <xsl:apply-templates select="document(@xlink:href)"/>
    </xsl:template>

    <!-- We make all URI references absolute. -->
    <xsl:template match="@xlink:href">
        <xsl:attribute name="xlink:href" select="resolve-uri(., base-uri())"/>
    </xsl:template>

    <xsl:template match="@*|node()">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    </xsl:template>

</xsl:stylesheet>
