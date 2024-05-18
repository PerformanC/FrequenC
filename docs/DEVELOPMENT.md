# FrequenC API Development

The FrequenC API is the main way of communication between the client and the server, and it's used for sending commands, and receiving events. This document will document which endpoints are in development, which are already implemented, and which are planned.

## Table of Contents

- [FrequenC API Development](#frequenc-api-development)
  - [Table of Contents](#table-of-contents)
  - [GET /v1/websocket - WebSocket](#get-v1websocket---websocket)
  - [GET /version - FrequenC version](#get-version---frequenc-version)
  - [GET /v1/info - Information about the node](#get-v1info---information-about-the-node)
  - [POST /v1/decodetracks - Bulk decode tracks](#post-v1decodetracks---bulk-decode-tracks)
  - [GET /v1/decodetrack - Decode a track](#get-v1decodetrack---decode-a-track)
  - [POST /v1/encodetracks - Bulk encode tracks](#post-v1encodetracks---bulk-encode-tracks)
  - [GET /v1/encodetrack - Encode a track](#get-v1encodetrack---encode-a-track)
  - [GET /v1/loadtracks - Load tracks](#get-v1loadtracks---load-tracks)
  - [GET /v1/sessions - Fetch all sessions](#get-v1sessions---fetch-all-sessions)
    - [GET, DELETE, PATCH /v1/sessions/xxx - Fetch a session](#get-delete-patch-v1sessionsxxx---fetch-a-session)
      - [GET /v1/sessions/xxx/players - Fetch all players](#get-v1sessionsxxxplayers---fetch-all-players)
        - [GET, DELETE, PATCH /v1/sessions/xxx/players/xxx - Fetch a player](#get-delete-patch-v1sessionsxxxplayersxxx---fetch-a-player)

> [!IMPORTANT]
> This is a beta version of the API.

## GET /v1/websocket - WebSocket

- [x] Development
- [x] Implemented
- [ ] Planned

> [!NOTE]
> Resuming support is expected, but not confirmed.

## GET /version - FrequenC version

- [ ] Development
- [x] Implemented
- [ ] Planned

> [!NOTE]
> This endpoint is considered stable, and is extremely unlikely to change between versions.

## GET /v1/info - Information about the node

- [x] Development
- [x] Implemented
- [ ] Planned

## POST /v1/decodetracks - Bulk decode tracks

- [x] Development
- [x] Implemented
- [ ] Planned

## GET /v1/decodetrack - Decode a track

- [x] Development
- [x] Implemented
- [ ] Planned

## POST /v1/encodetracks - Bulk encode tracks

- [x] Development
- [x] Implemented
- [ ] Planned

## GET /v1/encodetrack - Encode a track

- [x] Development
- [x] Implemented
- [ ] Planned

## GET /v1/loadtracks - Load tracks

- [x] Development
- [x] Implemented
- [ ] Planned

## GET /v1/sessions - Fetch all sessions

- [ ] Development
- [ ] Implemented
- [x] Planned

> [!NOTE]
> There isn't a guarantee that it will be implemented.

### GET, DELETE, PATCH /v1/sessions/xxx - Fetch a session

- [ ] Development
- [ ] Implemented
- [x] Planned

> [!NOTE]
> There isn't a guarantee that it will be implemented.

#### GET /v1/sessions/xxx/players - Fetch all players

- [x] Development
- [x] Implemented
- [ ] Planned

> [!NOTE]
> There isn't a guarantee that it will be implemented.

##### GET, DELETE, PATCH /v1/sessions/xxx/players/xxx - Fetch a player

- [x] Development
- [x] Implemented
- [ ] Planned
