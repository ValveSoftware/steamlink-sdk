<xsl:stylesheet xmlns:ex="http://example.com/NS" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">
    <xsl:template match="/">root-template</xsl:template>
    <xsl:template name="main">named-template</xsl:template>
    <xsl:template name="ex:main">namespaced-template</xsl:template>
</xsl:stylesheet>
