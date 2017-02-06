<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru">
<context>
    <name>QQmlWebSocket</name>
    <message>
        <source>Messages can only be sent when the socket is open.</source>
        <translation>Сообщения можно отправлять только при открытом сокете.</translation>
    </message>
    <message>
        <source>QQmlWebSocket is not ready.</source>
        <translation>QQmlWebSocket не готов.</translation>
    </message>
</context>
<context>
    <name>QQmlWebSocketServer</name>
    <message>
        <source>QQmlWebSocketServer is not ready.</source>
        <translation>QQmlWebSocketServer не готов.</translation>
    </message>
</context>
<context>
    <name>QWebSocket</name>
    <message>
        <source>Connection closed</source>
        <translation>Соединение закрыто</translation>
    </message>
    <message>
        <source>Invalid URL.</source>
        <translation>Неверный URL.</translation>
    </message>
    <message>
        <source>Invalid resource name.</source>
        <translation>Неверное имя ресурса.</translation>
    </message>
    <message>
        <source>SSL Sockets are not supported on this platform.</source>
        <translation>SSL сокеты не поддерживаются на данной платформе.</translation>
    </message>
    <message>
        <source>Out of memory.</source>
        <translation>Не хватает памяти.</translation>
    </message>
    <message>
        <source>Unsupported WebSocket scheme: %1</source>
        <translation>Неподдерживаемая схема WebSocket: %1</translation>
    </message>
    <message>
        <source>Error writing bytes to socket: %1.</source>
        <translation>Ошибка записи данных в сокет: %1.</translation>
    </message>
    <message>
        <source>Bytes written %1 != %2.</source>
        <translation>Записано байт %1 != %2.</translation>
    </message>
    <message>
        <source>Invalid statusline in response: %1.</source>
        <translation>Неверная строка состояния ответа: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Connection closed while reading header.</source>
        <translation>QWebSocketPrivate::processHandshake: Подключение закрыто при чтения заголовка.</translation>
    </message>
    <message>
        <source>Accept-Key received from server %1 does not match the client key %2.</source>
        <translation>Accept-Key, полученный от сервера %1, не совпадает с ключём клиента %2.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Invalid statusline in response: %1.</source>
        <translation>QWebSocketPrivate::processHandshake: Неверная строка состояния ответа: %1.</translation>
    </message>
    <message>
        <source>Handshake: Server requests a version that we don&apos;t support: %1.</source>
        <translation>Handshake: Сервер запрашивает неподдерживаемую версию: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.</source>
        <translation>QWebSocketPrivate::processHandshake: Возникла неизвестная ошибка. Отмена подключения.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unhandled http status code: %1 (%2).</source>
        <translation>QWebSocketPrivate::processHandshake: Необработанный код статуса HTTP: %1 (%2).</translation>
    </message>
    <message>
        <source>The resource name contains newlines. Possible attack detected.</source>
        <translation>Имя ресурса содержит переводы строк. Возможно, это атака.</translation>
    </message>
    <message>
        <source>The hostname contains newlines. Possible attack detected.</source>
        <translation>Hostname содержит переводы строк. Возможно, это атака.</translation>
    </message>
    <message>
        <source>The origin contains newlines. Possible attack detected.</source>
        <translation>Origin содержит переводы строк. Возможно, это атака.</translation>
    </message>
    <message>
        <source>The extensions attribute contains newlines. Possible attack detected.</source>
        <translation>Аттрибут Extensions содержит переводы строк. Возможно, это атака.</translation>
    </message>
    <message>
        <source>The protocols attribute contains newlines. Possible attack detected.</source>
        <translation>Аттрибут Protocols содержит переводы строк. Возможно, это атака.</translation>
    </message>
</context>
<context>
    <name>QWebSocketDataProcessor</name>
    <message>
        <source>Received Continuation frame, while there is nothing to continue.</source>
        <translation>Получен кадр продолжения, тогда как продолжать нечего.</translation>
    </message>
    <message>
        <source>All data frames after the initial data frame must have opcode 0 (continuation).</source>
        <translation>Все кадры данных после начального должны иметь код операции 0 (продолжение).</translation>
    </message>
    <message>
        <source>Received message is too big.</source>
        <translation>Полученное сообщение слишком большое.</translation>
    </message>
    <message>
        <source>Invalid UTF-8 code encountered.</source>
        <translation>Встречен неверный код UTF-8.</translation>
    </message>
    <message>
        <source>Payload of close frame is too small.</source>
        <translation>Размер закрывающего кадра слишком маленький.</translation>
    </message>
    <message>
        <source>Invalid close code %1 detected.</source>
        <translation>Обнаружен неверный код закрытия (%1).</translation>
    </message>
    <message>
        <source>Invalid opcode detected: %1</source>
        <translation>Обнаружен неверный код операции: %1</translation>
    </message>
</context>
<context>
    <name>QWebSocketFrame</name>
    <message>
        <source>Timeout when reading data from socket.</source>
        <translation>Истекло время ожидания при чтении данных из сокета.</translation>
    </message>
    <message>
        <source>Error occurred while reading from the network: %1</source>
        <translation>Возникла ошибка при чтении из сети: %1</translation>
    </message>
    <message>
        <source>Lengths smaller than 126 must be expressed as one byte.</source>
        <translation>Длины меньшие 126 должны быть представлены одним байтом.</translation>
    </message>
    <message>
        <source>Something went wrong during reading from the network.</source>
        <translation>Что-то пошло не так при чтении из сети.</translation>
    </message>
    <message>
        <source>Highest bit of payload length is not 0.</source>
        <translation>Старший бит размера данных не 0.</translation>
    </message>
    <message>
        <source>Lengths smaller than 65536 (2^16) must be expressed as 2 bytes.</source>
        <translation>Длины меньшие 65536 (2^16) должны быть представлены двумя байтами.</translation>
    </message>
    <message>
        <source>Error while reading from the network: %1.</source>
        <translation>Ошибка при чтении из сети: %1.</translation>
    </message>
    <message>
        <source>Maximum framesize exceeded.</source>
        <translation>Превышен размер кадра.</translation>
    </message>
    <message>
        <source>Some serious error occurred while reading from the network.</source>
        <translation>Возникла серьёзная ошибка при чтении из сети.</translation>
    </message>
    <message>
        <source>Rsv field is non-zero</source>
        <translation>Поле RSV не нулевое</translation>
    </message>
    <message>
        <source>Used reserved opcode</source>
        <translation>Используется резервный код операции</translation>
    </message>
    <message>
        <source>Control frame is larger than 125 bytes</source>
        <translation>Управляющий кадр больше 125 байт</translation>
    </message>
    <message>
        <source>Control frames cannot be fragmented</source>
        <translation>Управляющие кадры нельзя фрагментировать</translation>
    </message>
</context>
<context>
    <name>QWebSocketHandshakeResponse</name>
    <message>
        <source>Access forbidden.</source>
        <translation>Доступ запрещён.</translation>
    </message>
    <message>
        <source>Unsupported version requested.</source>
        <translation>Запрошена неподдерживаемая версия.</translation>
    </message>
    <message>
        <source>One of the headers contains a newline. Possible attack detected.</source>
        <translation>Один из заголовков содержит переводы строк. Возможно, это атака.</translation>
    </message>
    <message>
        <source>Bad handshake request received.</source>
        <translation>Получен не верный запрос рукопожатия.</translation>
    </message>
</context>
<context>
    <name>QWebSocketServer</name>
    <message>
        <source>Server closed.</source>
        <translation>Сервер закрыт.</translation>
    </message>
    <message>
        <source>Too many pending connections.</source>
        <translation>Слишком много ожидающих подключений.</translation>
    </message>
    <message>
        <source>Upgrade to WebSocket failed.</source>
        <translation>Не удалось повысить до WebSocket.</translation>
    </message>
    <message>
        <source>Invalid response received.</source>
        <translation>Получен неверный запрос.</translation>
    </message>
</context>
</TS>
