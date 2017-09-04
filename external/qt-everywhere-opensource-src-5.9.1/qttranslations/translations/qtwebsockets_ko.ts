<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="ko_KR">
<context>
    <name>QQmlWebSocket</name>
    <message>
        <source>Messages can only be sent when the socket is open.</source>
        <translation>소켓이 열려 있을 때에만 메시지를 전송할 수 있습니다.</translation>
    </message>
    <message>
        <source>QQmlWebSocket is not ready.</source>
        <translation>QQmlWebSocket이 준비되지 않았습니다.</translation>
    </message>
</context>
<context>
    <name>QQmlWebSocketServer</name>
    <message>
        <source>QQmlWebSocketServer is not ready.</source>
        <translation>QQmlWebSocketServer가 준비되지 않았습니다.</translation>
    </message>
</context>
<context>
    <name>QWebSocket</name>
    <message>
        <source>Connection closed</source>
        <translation>연결 종료됨</translation>
    </message>
    <message>
        <source>Invalid URL.</source>
        <translation>URL이 잘못되었습니다.</translation>
    </message>
    <message>
        <source>Invalid resource name.</source>
        <translation>자원 이름이 잘못되었습니다.</translation>
    </message>
    <message>
        <source>SSL Sockets are not supported on this platform.</source>
        <translation>이 플랫폼에서는 SSL 소켓을 지원하지 않습니다.</translation>
    </message>
    <message>
        <source>Out of memory.</source>
        <translation>메모리가 부족합니다.</translation>
    </message>
    <message>
        <source>Unsupported WebSocket scheme: %1</source>
        <translation>지원하지 않는 WebSocket 형식: %1</translation>
    </message>
    <message>
        <source>Error writing bytes to socket: %1.</source>
        <translation>소켓에 바이트를 쓰는 중 오류 발생: %1.</translation>
    </message>
    <message>
        <source>Bytes written %1 != %2.</source>
        <translation>쓴 바이트 %1 != %2입니다.</translation>
    </message>
    <message>
        <source>Invalid statusline in response: %1.</source>
        <translation>응답에 잘못된 상태 줄이 있음: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Connection closed while reading header.</source>
        <translation>QWebSocketPrivate::processHandshake: 헤더를 읽는 중 연결이 종료되었습니다.</translation>
    </message>
    <message>
        <source>Accept-Key received from server %1 does not match the client key %2.</source>
        <translation>서버 %1에서 받은 Accept-Key가 클라이언트 키 %2와(과) 다릅니다.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Invalid statusline in response: %1.</source>
        <translation>QWebSocketPrivate::processHandshake: 응답에 잘못된 상태 줄이 있음: %1.</translation>
    </message>
    <message>
        <source>Handshake: Server requests a version that we don&apos;t support: %1.</source>
        <translation>Handshake: 서버에서 지원하지 않는 버전을 요청함: %1.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.</source>
        <translation>QWebSocketPrivate::processHandshake: 알 수 없는 오류 조건입니다. 연결을 중단합니다.</translation>
    </message>
    <message>
        <source>QWebSocketPrivate::processHandshake: Unhandled http status code: %1 (%2).</source>
        <translation>QWebSocketPrivate::processHandshake: 처리되지 않은 HTTP 상태 코드: %1 (%2).</translation>
    </message>
    <message>
        <source>The resource name contains newlines. Possible attack detected.</source>
        <translation>자원 이름에 빈 줄이 있습니다. 공격 시도일 수 있습니다.</translation>
    </message>
    <message>
        <source>The hostname contains newlines. Possible attack detected.</source>
        <translation>호스트 이름에 빈 줄이 있습니다. 공격 시도일 수 있습니다.</translation>
    </message>
    <message>
        <source>The origin contains newlines. Possible attack detected.</source>
        <translation>원본에 빈 줄이 있습니다. 공격 시도일 수 있습니다.</translation>
    </message>
    <message>
        <source>The extensions attribute contains newlines. Possible attack detected.</source>
        <translation>확장 속성에 빈 줄이 있습니다. 공격 시도일 수 있습니다.</translation>
    </message>
    <message>
        <source>The protocols attribute contains newlines. Possible attack detected.</source>
        <translation>프로토콜 속성에 빈 줄이 있습니다. 공격 시도일 수 있습니다.</translation>
    </message>
</context>
<context>
    <name>QWebSocketDataProcessor</name>
    <message>
        <source>Received Continuation frame, while there is nothing to continue.</source>
        <translation>계속할 것이 없으나 계속 진행 프레임을 받았습니다.</translation>
    </message>
    <message>
        <source>All data frames after the initial data frame must have opcode 0 (continuation).</source>
        <translation>첫 데이터 후 모든 데이터 프레임은 동작 코드 0(계속 진행)이어야 합니다.</translation>
    </message>
    <message>
        <source>Received message is too big.</source>
        <translation>받은 메시지가 너무 큽니다.</translation>
    </message>
    <message>
        <source>Invalid UTF-8 code encountered.</source>
        <translation>잘못된 UTF-8 코드입니다.</translation>
    </message>
    <message>
        <source>Payload of close frame is too small.</source>
        <translation>닫기 프레임의 페이로드가 너무 작습니다.</translation>
    </message>
    <message>
        <source>Invalid close code %1 detected.</source>
        <translation>잘못된 닫기 코드 %1입니다.</translation>
    </message>
    <message>
        <source>Invalid opcode detected: %1</source>
        <translation>잘못된 동작 코드 감지됨: %1</translation>
    </message>
</context>
<context>
    <name>QWebSocketFrame</name>
    <message>
        <source>Timeout when reading data from socket.</source>
        <translation>소켓에서 데이터를 읽는 중 시간을 초과했습니다.</translation>
    </message>
    <message>
        <source>Error occurred while reading from the network: %1</source>
        <translation>네트워크에서 읽는 중 오류 발생: %1</translation>
    </message>
    <message>
        <source>Lengths smaller than 126 must be expressed as one byte.</source>
        <translation>126보다 작은 크기는 1바이트로 표현해야 합니다.</translation>
    </message>
    <message>
        <source>Something went wrong during reading from the network.</source>
        <translation>네트워크에서 읽는 중 무언가 잘못되었습니다.</translation>
    </message>
    <message>
        <source>Highest bit of payload length is not 0.</source>
        <translation>페이로드 크기의 최상위 비트가 0이 아닙니다.</translation>
    </message>
    <message>
        <source>Lengths smaller than 65536 (2^16) must be expressed as 2 bytes.</source>
        <translation>65536(2^16)보다 작은 크기는 2바이트로 표현해야 합니다.</translation>
    </message>
    <message>
        <source>Error while reading from the network: %1.</source>
        <translation>네트워크에서 읽는 중 오류 발생: %1.</translation>
    </message>
    <message>
        <source>Maximum framesize exceeded.</source>
        <translation>최대 프레임 크기를 초과했습니다.</translation>
    </message>
    <message>
        <source>Some serious error occurred while reading from the network.</source>
        <translation>네트워크에서 읽는 중 치명적인 오류가 발생했습니다.</translation>
    </message>
    <message>
        <source>Rsv field is non-zero</source>
        <translation>Rsv 필드가 0이 아님</translation>
    </message>
    <message>
        <source>Used reserved opcode</source>
        <translation>예약된 동작 코드 사용</translation>
    </message>
    <message>
        <source>Control frame is larger than 125 bytes</source>
        <translation>컨트롤 프레임이 125바이트보다 큼</translation>
    </message>
    <message>
        <source>Control frames cannot be fragmented</source>
        <translation>컨트롤 프레임은 분할할 수 없음</translation>
    </message>
</context>
<context>
    <name>QWebSocketHandshakeResponse</name>
    <message>
        <source>Access forbidden.</source>
        <translation>접근이 거부되었습니다.</translation>
    </message>
    <message>
        <source>Unsupported version requested.</source>
        <translation>지원하지 않는 버전을 요청했습니다.</translation>
    </message>
    <message>
        <source>One of the headers contains a newline. Possible attack detected.</source>
        <translation>헤더에 빈 줄이 있습니다. 공격 시도일 수 있습니다.</translation>
    </message>
    <message>
        <source>Bad handshake request received.</source>
        <translation>잘못된 Handshake 요청을 받았습니다.</translation>
    </message>
</context>
<context>
    <name>QWebSocketServer</name>
    <message>
        <source>Server closed.</source>
        <translation>서버에서 연결을 종료했습니다.</translation>
    </message>
    <message>
        <source>Too many pending connections.</source>
        <translation>대기 중인 연결이 너무 많습니다.</translation>
    </message>
    <message>
        <source>Upgrade to WebSocket failed.</source>
        <translation>WebSocket으로 업그레이드할 수 없습니다.</translation>
    </message>
    <message>
        <source>Invalid response received.</source>
        <translation>잘못된 응답을 받았습니다.</translation>
    </message>
</context>
</TS>
