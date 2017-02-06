<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:xs="http://www.w3.org/2001/XMLSchema"
                version="2.0">

    <!-- QXmlQuery::bindVariable() never overrides xsl:variable. -->
    <xsl:variable name="variableNoSelectNoBody"/>
    <xsl:variable name="variableNoSelectNoBodyBoundWithBindVariable"/>      <!-- Test calls bindVariable() for this. -->
    <xsl:variable name="variableSelect" select="'variableSelectsDefaultValue'"/>
    <xsl:variable name="variableSelectBoundWithBindVariable" select="'variableSelectsDefaultValue2'"/> <!-- Test calls bindVariable() for this. -->
    <xsl:variable name="variableSelectWithTypeInt" as="xs:integer" select="3"/>
    <xsl:variable name="variableSelectWithTypeIntBoundWithBindVariable" as="xs:integer" select="4"/>        <!-- Test calls bindVariable() for this. -->

    <xsl:param name="paramNoSelectNoBody"/>
    <xsl:param name="paramNoSelectNoBodyBoundWithBindVariable"/> <!-- Test calls bindVariable() for this. -->
    <xsl:param name="paramNoSelectNoBodyBoundWithBindVariableRequired" required="yes"/> <!-- Test calls bindVariable() for this. -->
    <xsl:param name="paramSelect" select="'variableSelectsDefaultValue'"/>
    <xsl:param name="paramSelectBoundWithBindVariable" select="'variableSelectsDefaultValue'"/> <!-- Test calls bindVariable() for this. -->
    <xsl:param name="paramSelectBoundWithBindVariableRequired"  required="yes"/> <!-- Test calls bindVariable() for this. -->
    <xsl:param name="paramSelectWithTypeInt" as="xs:integer" select="1"/>
    <xsl:param name="paramSelectWithTypeIntBoundWithBindVariable" as="xs:integer" select="1"/> <!-- Test calls bindVariable() for this. -->
    <xsl:param name="paramSelectWithTypeIntBoundWithBindVariableRequired" required="yes" as="xs:integer"/> <!-- Test calls bindVariable() for this. -->

    <xsl:template name="main">
        <xsl:sequence select="'Variables:',
                              $variableNoSelectNoBody,
                              $variableNoSelectNoBodyBoundWithBindVariable,
                              $variableSelect,
                              $variableSelectBoundWithBindVariable,
                              $variableSelectWithTypeInt,
                              $variableSelectWithTypeIntBoundWithBindVariable,
                              'Parameters:',
                              $paramNoSelectNoBodyBoundWithBindVariable,
                              $paramNoSelectNoBodyBoundWithBindVariableRequired,
                              $paramSelectBoundWithBindVariable,
                              $paramSelectBoundWithBindVariableRequired,
                              $paramSelectWithTypeIntBoundWithBindVariable,
                              $paramSelectWithTypeIntBoundWithBindVariableRequired"/>
    </xsl:template>

</xsl:stylesheet>

