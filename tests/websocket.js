import WebSocket from '@performanc/pwsl-mini'

const ws = new WebSocket('ws://127.0.0.1:8888/v1/websocket', {
  headers: {
    Authorization: 'youshallnotpass',
    'User-Id': '111111111111111111',
    'Client-Info': 'Custom Client/1.0.0 (PerfomanC\'s Bot)',
  }
})

ws.on('open', () => {
  console.log('[WebSocket]: Successfully connected.')
})

ws.on('message', (data) => {
  console.log('[WebSocket]: Received message:', data)
})

ws.on('close', () => {
  console.log('[WebSocket]: Connection closed.')
})