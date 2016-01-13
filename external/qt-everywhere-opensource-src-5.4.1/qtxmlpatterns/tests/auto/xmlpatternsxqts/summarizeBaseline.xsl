<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:m="http://www.w3.org/2005/02/query-test-XQTSResult">

    <!-- Simply outputs a list of what test cases that failed. -->

    <xsl:output method="text"/>

    <xsl:template match="/">
        <xsl:text>Failures:&#10;</xsl:text>
        <xsl:apply-templates select="m:test-suite-result/m:test-case">
            <xsl:sort select="@name"/>
        </xsl:apply-templates>
    </xsl:template>

    <xsl:template match="m:test-case[@result = 'fail']">
        <xsl:text>    </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
    </xsl:template>

</xsl:stylesheet>
<!-- vim: et:ts=4:sw=4:sts=4
-->
