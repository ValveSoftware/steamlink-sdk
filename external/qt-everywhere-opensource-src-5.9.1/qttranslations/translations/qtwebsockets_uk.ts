<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="uk_UA">
<context>
    <name>QQmlWebSocket</name>
    <message>
        <source>Messages can only be sent when the socket is open.</source>
        <translation>Повідомлення можуть бути надіслані, лише коли сокет відкрито.</translation>
    </message>
    <message>
        <source>QQmlWebSocket is not ready.</source>
        <translation>QQmlWebSocket не готовий.</translation>
    </message>
</context>
<context>
    <name>QQmlWebSocketServer</name>
    <message>
        <source>QQmlWebSocketServer is not ready.</source>
        <translation>QQmlWebSocketServer не готовий.</translation>
    </message>
</context>
<context>
    <name>QWebSocket</name>
    <message>
        <source>Connection closed</source>
        <translation>З&apos;єднання закрито</translation>
    </message>
    <message>
        <source>Invalid URL.</source>
        <translation>Неправильний URL.</translation>
    </message>
    <message>
        <source>Invalid resource name.</source>
        <translation>Неправильна назва ресурсу.</translation>
    </message>
    <message>
        <source>SSL Sockets are not supported on this platform.</source>
        <translation>Сокети SSL не підтримуються на цій платформі.</translation>
    </message>
    <message>
        <source>Out of memory.</source>
        <translation>Не вистачає пам&apos;яті.</translation>
    </message>
    <message>
        <source>Unsupported WebSocket scheme: %1</source>
        <translation>Непідтримувана схема WebSocket: %1</translation>
    </message>
    <message>
        <source>Error writing bytes to socket: %1.</source>
        <translation>Помилка запису байтів до сокета: %1.</translation>
    </message>
    <message>
        <source>Bytes written %1 != %2.</source>
        <translation>Кількість записаних байтів не збігається: %1 != %2.</translation>
    </message>
    <message>
        <source>Invalid statusline in response: %1.</source>
        <translation>Неправильний статусний рядок у відповіді: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Connection closed while reading header.</source>
        <translation>QWebSocketPrivate::processHandshake: з&apos;єднання було закрите під час читання заголовків.</translation>
    </message>
    <message>
        <source>Accept-Key received from server %1 does not match the client key %2.</source>
        <translation>Accept-Key, отриманий від сервера %1, не збігається з клієнтським ключем %2.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Invalid statusline in response: %1.</source>
        <translation>QWebSocketPrivate::processHandshake: неправильний статусний рядок у відповіді: %1.</translation>
    </message>
    <message>
        <source>Handshake: Server requests a version that we don&apos;t support: %1.</source>
        <translation>Рукостискання: сервер вимає версію, яку ми не підтримуємо: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.</source>
        <translation>QWebSocketPrivate::processHandshake: Трапилась невідома помилка. З&apos;єднання перервано.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unhandled http status code: %1 (%2).</source>
        <translation>QWebSocketPrivate::processHandshake: непідтримуваний код статусу HTTP: %1 (%2).</translation>
    </message>
    <message>
        <source>The resource name contains newlines. Possible attack detected.</source>
        <translation>Назва ресурсу містисть порожній рядок. Виявлено спробу нападу.</translation>
    </message>
    <message>
        <source>The hostname contains newlines. Possible attack detected.</source>
        <translation>Назва вузла містисть порожній рядок. Виявлено спробу нападу.</translation>
    </message>
    <message>
        <source>The origin contains newlines. Possible attack detected.</source>
        <translation>Заголовок Origin містисть порожній рядок. Виявлено спробу нападу.</translation>
    </message>
    <message>
        <source>The extensions attribute contains newlines. Possible attack detected.</source>
        <translation>Заголовок Sec-WebSocket-Extensions містить порожній рядок. Виявлено спробу нападу.</translation>
    </message>
    <message>
        <source>The protocols attribute contains newlines. Possible attack detected.</source>
        <translation>Заголовок Sec-WebSocket-Protocol містить порожній рядок. Виявлено спробу нападу.</translation>
    </message>
</context>
<context>
    <name>QWebSocketDataProcessor</name>
    <message>
        <source>Received Continuation frame, while there is nothing to continue.</source>
        <translation>Отримано кадр Continuation, але немає чого продовжувати.</translation>
    </message>
    <message>
        <source>All data frames after the initial data frame must have opcode 0 (continuation).</source>
        <translation>Усі кадри даних після початкового, мусять мати опкод 0 (продовження).</translation>
    </message>
    <message>
        <source>Received message is too big.</source>
        <translation>Отримано завелике повідомлення.</translation>
    </message>
    <message>
        <source>Invalid UTF-8 code encountered.</source>
        <translation>Трапився неправильний код UTF-8.</translation>
    </message>
    <message>
        <source>Payload of close frame is too small.</source>
        <translation>Замало даних в кадрі закриття.</translation>
    </message>
    <message>
        <source>Invalid close code %1 detected.</source>
        <translation>Виявлено неправильний код закриття %1.</translation>
    </message>
    <message>
        <source>Invalid opcode detected: %1</source>
        <translation>Виявлено неправильний опкод: %1</translation>
    </message>
</context>
<context>
    <name>QWebSocketFrame</name>
    <message>
        <source>Timeout when reading data from socket.</source>
        <translation>Час очікування вичерпано під час читання з сокета.</translation>
    </message>
    <message>
        <source>Error occurred while reading from the network: %1</source>
        <translation>Під час читання з мережі сталась помилка: %1</translation>
    </message>
    <message>
        <source>Lengths smaller than 126 must be expressed as one byte.</source>
        <translation>Довжини менші за 126 мають бути представлені як один байт.</translation>
    </message>
    <message>
        <source>Something went wrong during reading from the network.</source>
        <translation>Щось пішло не так під час читання з мережі.</translation>
    </message>
    <message>
        <source>Highest bit of payload length is not 0.</source>
        <translation>Найстарший біт довжини даних не є 0.</translation>
    </message>
    <message>
        <source>Lengths smaller than 65536 (2^16) must be expressed as 2 bytes.</source>
        <translation>Довжини менші за  65536 (2^16) мають бути представлені як 2 байти.</translation>
    </message>
    <message>
        <source>Error while reading from the network: %1.</source>
        <translation>Помилка читання з мережі: %1.</translation>
    </message>
    <message>
        <source>Maximum framesize exceeded.</source>
        <translation>Перевищено максимальний розмір кадру.</translation>
    </message>
    <message>
        <source>Some serious error occurred while reading from the network.</source>
        <translation>Під час читання з мережі сталась якась поважна помилка.</translation>
    </message>
    <message>
        <source>Rsv field is non-zero</source>
        <translation>Поле Rsv не нульове</translation>
    </message>
    <message>
        <source>Used reserved opcode</source>
        <translation>Використано зарезервований опкод</translation>
    </message>
    <message>
        <source>Control frame is larger than 125 bytes</source>
        <translation>Управляючий кадр більший ніж 125 байтів</translation>
    </message>
    <message>
        <source>Control frames cannot be fragmented</source>
        <translation>Управляючі кадри не можуть бути фрагментовані</translation>
    </message>
</context>
<context>
    <name>QWebSocketHandshakeResponse</name>
    <message>
        <source>Access forbidden.</source>
        <translation>Доступ заборонено.</translation>
    </message>
    <message>
        <source>Unsupported version requested.</source>
        <translation>Запитано версію, що не підтримується.</translation>
    </message>
    <message>
        <source>One of the headers contains a newline. Possible attack detected.</source>
        <translation>Один із заголовків містить порожній рядок. Виявлено спробу нападу.</translation>
    </message>
    <message>
        <source>Bad handshake request received.</source>
        <translation>Отримано неправильний запит на рукостискання.</translation>
    </message>
</context>
<context>
    <name>QWebSocketServer</name>
    <message>
        <source>Server closed.</source>
        <translation>Сервер закрито.</translation>
    </message>
    <message>
        <source>Too many pending connections.</source>
        <translation>Забагато з&apos;єднань, що очікують.</translation>
    </message>
    <message>
        <source>Upgrade to WebSocket failed.</source>
        <translation>Збій оновлення до WebSocket.</translation>
    </message>
    <message>
        <source>Invalid response received.</source>
        <translation>Отримано неправильну відповідь.</translation>
    </message>
</context>
</TS>
