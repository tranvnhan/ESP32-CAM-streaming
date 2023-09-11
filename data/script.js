window.addEventListener('load', onLoad)

// Using Websocket for communication between server and clients
var gateway = `ws://${location.host}/ws`;
var websocket;

function onLoad(event) {
    baseHost = document.location.origin
    streamUrl = baseHost

    view = document.getElementById('stream')
    viewContainer = document.getElementById('stream-container')

    initButton();

    initWebSocket();
}

function initButton() {
    document.getElementById('toggle-stream').addEventListener('click', toggle_stream)
}
function toggle_stream() {
    console.log("Toggle clicked")
    console.log(streamUrl)

    view.src = `${streamUrl}/stream`
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onWsOpen;
    websocket.onclose = onWsClose;
    // websocket.onmessage = onWsMessage; // <-- add this line
}

function onWsOpen(event) {
    console.log('Connection opened');
}

function onWsClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}