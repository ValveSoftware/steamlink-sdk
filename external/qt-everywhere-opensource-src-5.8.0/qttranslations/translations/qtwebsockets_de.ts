<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE">
<context>
    <name>QQmlWebSocket</name>
    <message>
        <source>Messages can only be sent when the socket is open.</source>
        <translation>Nachrichten können nur versandt werden, wenn der Socket geöffnet ist.</translation>
    </message>
    <message>
        <source>QQmlWebSocket is not ready.</source>
        <translation>QQmlWebSocket nicht bereit.</translation>
    </message>
</context>
<context>
    <name>QQmlWebSocketServer</name>
    <message>
        <source>QQmlWebSocketServer is not ready.</source>
        <translation>QQmlWebSocketServer nicht bereit.</translation>
    </message>
</context>
<context>
    <name>QWebSocket</name>
    <message>
        <source>Connection closed</source>
        <translation>Verbindung geschlossen</translation>
    </message>
    <message>
        <source>Invalid URL.</source>
        <translation>Ungültiger URL.</translation>
    </message>
    <message>
        <source>Invalid resource name.</source>
        <translation>Ungültiger Ressourcenname.</translation>
    </message>
    <message>
        <source>SSL Sockets are not supported on this platform.</source>
        <translation>SSL-Sockets sind auf dieser Plattform nicht unterstützt.</translation>
    </message>
    <message>
        <source>Out of memory.</source>
        <translation>Kein freier Speicher mehr vorhanden.</translation>
    </message>
    <message>
        <source>Unsupported WebSocket scheme: %1</source>
        <translation>WebSocket-Schema %1 ist nicht unterstützt</translation>
    </message>
    <message>
        <source>Error writing bytes to socket: %1.</source>
        <translation>Fehler beim Schreiben von Daten zum Socket: %1.</translation>
    </message>
    <message>
        <source>Bytes written %1 != %2.</source>
        <translation>Es konnten nicht alle Daten geschrieben werden: %1 != %2.</translation>
    </message>
    <message>
        <source>Invalid statusline in response: %1.</source>
        <translation>Ungültige Statuszeile in Antwort: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Connection closed while reading header.</source>
        <translation>QWebSocketPrivate::processHandshake: Verbindung während des Auslesens des Headers geschlossen.</translation>
    </message>
    <message>
        <source>Accept-Key received from server %1 does not match the client key %2.</source>
        <translation>Der Accept-Schlüssel vom Server %1 entspricht nicht dem Schlüssel des Clients %2.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Invalid statusline in response: %1.</source>
        <translation>QWebSocketPrivate::processHandshake: Ungültige Statuszeile in Antwort: %1.</translation>
    </message>
    <message>
        <source>Handshake: Server requests a version that we don&apos;t support: %1.</source>
        <translation>Handshake: Der Server verlangt eine nicht unterstützte Version: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.</source>
        <translation>QWebSocketPrivate::processHandshake: Es ist ein unbekannter Fehler aufgetreten. Verbindung beendet.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unhandled http status code: %1 (%2).</source>
        <translation>QWebSocketPrivate::processHandshake: Unbehandelter HTTP-Status-Code: %1 (%2).</translation>
    </message>
    <message>
        <source>The resource name contains newlines. Possible attack detected.</source>
        <translation>Der Ressourcenname enthält Zeilenvorschübe. Potentieller Angriff festgestellt.</translation>
    </message>
    <message>
        <source>The hostname contains newlines. Possible attack detected.</source>
        <translation>Der Hostname enthält Zeilenvorschübe. Potentieller Angriff festgestellt.</translation>
    </message>
    <message>
        <source>The origin contains newlines. Possible attack detected.</source>
        <translation>Die Quelle enthält Zeilenvorschübe. Potentieller Angriff festgestellt.</translation>
    </message>
    <message>
        <source>The extensions attribute contains newlines. Possible attack detected.</source>
        <translation>Das Attribut für Erweiterungen enthält Zeilenvorschübe. Potentieller Angriff festgestellt.</translation>
    </message>
    <message>
        <source>The protocols attribute contains newlines. Possible attack detected.</source>
        <translation>Das Attribut für Protokolle enthält Zeilenvorschübe. Potentieller Angriff festgestellt.</translation>
    </message>
</context>
<context>
    <name>QWebSocketDataProcessor</name>
    <message>
        <source>Received Continuation frame, while there is nothing to continue.</source>
        <translation>Continuation-Frame erhalten, der nicht zugeordnet werden kann.</translation>
    </message>
    <message>
        <source>All data frames after the initial data frame must have opcode 0 (continuation).</source>
        <translation>Alle dem ersten Daten-Frame folgenden Frames müssen den Opcode 0 haben (Continuation).</translation>
    </message>
    <message>
        <source>Received message is too big.</source>
        <translation>Die erhaltene Nachricht ist zu groß.</translation>
    </message>
    <message>
        <source>Invalid UTF-8 code encountered.</source>
        <translation>Es wurde ein ungültiger UTF-8-Code empfangen.</translation>
    </message>
    <message>
        <source>Payload of close frame is too small.</source>
        <translation>Payload des abschließenden Frames ist zu klein.</translation>
    </message>
    <message>
        <source>Invalid close code %1 detected.</source>
        <translation>Ungültiger Schluss-Code %1 festgestellt.</translation>
    </message>
    <message>
        <source>Invalid opcode detected: %1</source>
        <translation>Ungültiger Opcode %1 festgestellt</translation>
    </message>
</context>
<context>
    <name>QWebSocketFrame</name>
    <message>
        <source>Timeout when reading data from socket.</source>
        <translation>Zeitüberschreitung beim Lesen vom Socket.</translation>
    </message>
    <message>
        <source>Error occurred while reading from the network: %1</source>
        <translation>Fehler beim Lesen vom Netzwerk: %1</translation>
    </message>
    <message>
        <source>Lengths smaller than 126 must be expressed as one byte.</source>
        <translation>Längenangaben unter 126 müssen als ein Byte angegeben werden.</translation>
    </message>
    <message>
        <source>Something went wrong during reading from the network.</source>
        <translation>Fehler beim Lesen vom Netzwerk.</translation>
    </message>
    <message>
        <source>Highest bit of payload length is not 0.</source>
        <translation>Das höchste Bit der Payload-Länge ist nicht 0.</translation>
    </message>
    <message>
        <source>Lengths smaller than 65536 (2^16) must be expressed as 2 bytes.</source>
        <translation>Längenangaben unter 65536 (2^16) müssen als zwei Bytes angegeben werden.</translation>
    </message>
    <message>
        <source>Error while reading from the network: %1.</source>
        <translation>Fehler beim Lesen vom Netzwerk: %1.</translation>
    </message>
    <message>
        <source>Maximum framesize exceeded.</source>
        <translation>Maximale Größe des Frames überschritten.</translation>
    </message>
    <message>
        <source>Some serious error occurred while reading from the network.</source>
        <translation>Schwerwiegender Fehler beim Lesen vom Netzwerk.</translation>
    </message>
    <message>
        <source>Rsv field is non-zero</source>
        <translation>Das Rsv-Feld ist nicht null</translation>
    </message>
    <message>
        <source>Used reserved opcode</source>
        <translation>Reservierter Opcode verwendet</translation>
    </message>
    <message>
        <source>Control frame is larger than 125 bytes</source>
        <translation>Control-Frame ist größer als 125 Bytes</translation>
    </message>
    <message>
        <source>Control frames cannot be fragmented</source>
        <translation>Control-Frames können nicht aufgeteilt werden</translation>
    </message>
</context>
<context>
    <name>QWebSocketHandshakeResponse</name>
    <message>
        <source>Access forbidden.</source>
        <translation>Zugriff verboten.</translation>
    </message>
    <message>
        <source>Unsupported version requested.</source>
        <translation>Nicht unterstützte Version angefordert.</translation>
    </message>
    <message>
        <source>One of the headers contains a newline. Possible attack detected.</source>
        <translation>Einer der Header enthält Zeilenvorschübe. Potentieller Angriff festgestellt.</translation>
    </message>
    <message>
        <source>Bad handshake request received.</source>
        <translation>Ungültige Handshake-Anforderung erhalten.</translation>
    </message>
</context>
<context>
    <name>QWebSocketServer</name>
    <message>
        <source>Server closed.</source>
        <translation>Server geschlossen.</translation>
    </message>
    <message>
        <source>Too many pending connections.</source>
        <translation>Zu viele anstehende Verbindungen.</translation>
    </message>
    <message>
        <source>Upgrade to WebSocket failed.</source>
        <translation>Upgrade auf WebSocket fehlgeschlagen.</translation>
    </message>
    <message>
        <source>Invalid response received.</source>
        <translation>Ungültige Antwort erhalten.</translation>
    </message>
</context>
</TS>
