<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
  <html>
  <head><title>SameGame High Scores</title></head>
  <body>
  <h2>SameGame High Scores</h2>
    <table border="1">
      <tr bgcolor="lightsteelblue">
        <th>Name</th>
        <th>Score</th>
        <th>Grid Size</th>
        <th>Time, s</th>
      </tr>
      <xsl:for-each select="records/record">
      <xsl:sort select="score" data-type="number" order="descending"/>
      <tr>
        <td><xsl:value-of select="name"/></td>
        <td><xsl:value-of select="score"/></td>
        <td><xsl:value-of select="gridSize"/></td>
        <td><xsl:value-of select="seconds"/></td>
      </tr>
      </xsl:for-each>
    </table>
  </body>
  </html>
</xsl:template>
</xsl:stylesheet>
