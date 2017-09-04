<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ja_JP">
<context>
    <name>QQmlWebSocket</name>
    <message>
        <source>Messages can only be sent when the socket is open.</source>
        <translation>メッセージはソケットが開いている時のみ送信できます。</translation>
    </message>
    <message>
        <source>QQmlWebSocket is not ready.</source>
        <translation>QQmlWebSocket の準備ができていません。</translation>
    </message>
</context>
<context>
    <name>QQmlWebSocketServer</name>
    <message>
        <source>QQmlWebSocketServer is not ready.</source>
        <translation>QQmlWebSocketServer の準備ができていません。</translation>
    </message>
</context>
<context>
    <name>QWebSocket</name>
    <message>
        <source>Connection closed</source>
        <translation>切断しました</translation>
    </message>
    <message>
        <source>Invalid URL.</source>
        <translation>無効なURLです。</translation>
    </message>
    <message>
        <source>Invalid resource name.</source>
        <translation>無効なリソース名です。</translation>
    </message>
    <message>
        <source>SSL Sockets are not supported on this platform.</source>
        <translation>このプラットフォームでは SSL ソケットがサポートされていません。</translation>
    </message>
    <message>
        <source>Out of memory.</source>
        <translation>メモリが不足しています。</translation>
    </message>
    <message>
        <source>Unsupported WebSocket scheme: %1</source>
        <translation>サポートしていない WebSocket のスキーム: %1</translation>
    </message>
    <message>
        <source>Error writing bytes to socket: %1.</source>
        <translation>ソケットへの書込中のエラー: %1。</translation>
    </message>
    <message>
        <source>Bytes written %1 != %2.</source>
        <translation>書き込んだバイト数 %1 != %2.</translation>
    </message>
    <message>
        <source>Invalid statusline in response: %1.</source>
        <translation>応答に無効なステータス行があります: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Connection closed while reading header.</source>
        <translation>QWebSocketPrivate::processHandshake: ヘッダ読込中に切断されました。</translation>
    </message>
    <message>
        <source>Accept-Key received from server %1 does not match the client key %2.</source>
        <translation>サーバから受け取った Accept ヘッダの値 %1 がクライアントの Key の値 %2 と合致しません。</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Invalid statusline in response: %1.</source>
        <translation>QWebSocketPrivate::processHandshake: 応答に無効なステータス行があります: %1.</translation>
    </message>
    <message>
        <source>Handshake: Server requests a version that we don&apos;t support: %1.</source>
        <translation>Handshake: サーバが要求するバージョンはサポートしていません: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.</source>
        <translation>QWebSocketPrivate::processHandshake: 未知のエラーが発生しました。接続を中止します。</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unhandled http status code: %1 (%2).</source>
        <translation>QWebSocketPrivate::processHandshake: 処理対象ではない http ステータスコード: %1 (%2).</translation>
    </message>
    <message>
        <source>The resource name contains newlines. Possible attack detected.</source>
        <translation>リソース名に空行が含まれています。攻撃された可能性があります。</translation>
    </message>
    <message>
        <source>The hostname contains newlines. Possible attack detected.</source>
        <translation>ホスト名に空行が含まれています。攻撃された可能性があります。</translation>
    </message>
    <message>
        <source>The origin contains newlines. Possible attack detected.</source>
        <translation>生成元(Origin)に空行が含まれています。攻撃された可能性があります。</translation>
    </message>
    <message>
        <source>The extensions attribute contains newlines. Possible attack detected.</source>
        <translation>拡張属性に空行が含まれています。攻撃された可能性があります。</translation>
    </message>
    <message>
        <source>The protocols attribute contains newlines. Possible attack detected.</source>
        <translation>プロトコル属性に空行が含まれています。攻撃された可能性があります。</translation>
    </message>
</context>
<context>
    <name>QWebSocketDataProcessor</name>
    <message>
        <source>Received Continuation frame, while there is nothing to continue.</source>
        <translation>継続フレームを受け取りましたが、対応するフレームが存在しません。</translation>
    </message>
    <message>
        <source>All data frames after the initial data frame must have opcode 0 (continuation).</source>
        <translation>初期データフレーム後のすべてのデータフレームは opcode が 0 である必要があります(継続フレーム)。</translation>
    </message>
    <message>
        <source>Received message is too big.</source>
        <translation>受け取ったメッセージが大きすぎます。</translation>
    </message>
    <message>
        <source>Invalid UTF-8 code encountered.</source>
        <translation>無効な UTF-8 コードを検出しました。</translation>
    </message>
    <message>
        <source>Payload of close frame is too small.</source>
        <translation>Close フレームのペイロードが小さすぎます。</translation>
    </message>
    <message>
        <source>Invalid close code %1 detected.</source>
        <translation>無効な close コード %1 を検出しました。</translation>
    </message>
    <message>
        <source>Invalid opcode detected: %1</source>
        <translation>無効な opcode を検出しました: %1</translation>
    </message>
</context>
<context>
    <name>QWebSocketFrame</name>
    <message>
        <source>Timeout when reading data from socket.</source>
        <translation>ソケットからデータの読込中にタイムアウトしました。</translation>
    </message>
    <message>
        <source>Error occurred while reading from the network: %1</source>
        <translation>ネットワークから読込中にエラーが発生しました: %1</translation>
    </message>
    <message>
        <source>Lengths smaller than 126 must be expressed as one byte.</source>
        <translation>ペイロード長が126未満の場合、1バイトで表現されている必要があります。</translation>
    </message>
    <message>
        <source>Something went wrong during reading from the network.</source>
        <translation>ネットワークから読込中に問題が発生しました。</translation>
    </message>
    <message>
        <source>Highest bit of payload length is not 0.</source>
        <translation>ペイロード長の最上位ビットが0ではありません。</translation>
    </message>
    <message>
        <source>Lengths smaller than 65536 (2^16) must be expressed as 2 bytes.</source>
        <translation>ペイロード長が65536(2^16)未満の場合、2バイトで表現されている必要があります。</translation>
    </message>
    <message>
        <source>Error while reading from the network: %1.</source>
        <translation>ネットワークから読込中にエラーが発生しました: %1。</translation>
    </message>
    <message>
        <source>Maximum framesize exceeded.</source>
        <translation>最大フレームサイズを超えています。</translation>
    </message>
    <message>
        <source>Some serious error occurred while reading from the network.</source>
        <translation>ネットワークから読込中に深刻なエラーが発生しました。</translation>
    </message>
    <message>
        <source>Rsv field is non-zero</source>
        <translation>RSV の値が0ではありません</translation>
    </message>
    <message>
        <source>Used reserved opcode</source>
        <translation>予約済みの opcode が使用されました</translation>
    </message>
    <message>
        <source>Control frame is larger than 125 bytes</source>
        <translation>制御フレームのサイズが125バイトよりも大きい</translation>
    </message>
    <message>
        <source>Control frames cannot be fragmented</source>
        <translation>制御フレームは分割できません</translation>
    </message>
</context>
<context>
    <name>QWebSocketHandshakeResponse</name>
    <message>
        <source>Access forbidden.</source>
        <translation>アクセス権限がありません。</translation>
    </message>
    <message>
        <source>Unsupported version requested.</source>
        <translation>要求されたバージョンはサポートしていません。</translation>
    </message>
    <message>
        <source>One of the headers contains a newline. Possible attack detected.</source>
        <translation>ヘッダに空行が含まれています。攻撃された可能性があります。</translation>
    </message>
    <message>
        <source>Bad handshake request received.</source>
        <translation>不正なハンドシェーク要求を受け取りました。</translation>
    </message>
</context>
<context>
    <name>QWebSocketServer</name>
    <message>
        <source>Server closed.</source>
        <translation>サーバが閉じられました。</translation>
    </message>
    <message>
        <source>Too many pending connections.</source>
        <translation>接続待ちが多すぎます。</translation>
    </message>
    <message>
        <source>Upgrade to WebSocket failed.</source>
        <translation>WebSocket へのアップグレードに失敗しました。</translation>
    </message>
    <message>
        <source>Invalid response received.</source>
        <translation>無効な応答を受け取りました。</translation>
    </message>
</context>
</TS>
