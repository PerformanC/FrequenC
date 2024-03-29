# FrequenC API

The FrequenC API is the main way of communication between the client and the server, and it's used for sending commands, and receiving events.

## Table of Contents

- [FrequenC API](#frequenc-api)
  - [Information about server](#information-about-server)
  - [Fetching the version](#fetching-the-version)
  - [Decoding track](#decoding-track)
  - [Decoding tracks](#decoding-tracks)
  - [Encoding track](#encoding-track)
  - [Encoding tracks](#encoding-tracks)
  - [Loading tracks](#loading-tracks)
  - [Connecting to the websocket](#connecting-to-the-websocket)
  - [LavaLink compability table](#lavalink-compability-table)

> [!NOTE]
> All endpoints require authentication.

> [!WARNING]
> All endpoints, except `/version`, are prefixed with `/v1` and may change between versions.

## Information about server

The HTTP parser of FrequenC is implemented by The PeformanC Organization, and is used by FrequenC to parse HTTP requests. Through it, we added limitations to the HTTP server, and more strictness to the HTTP parser.

> [!WARNING]
> Headers amount must be not higher than 10.

> [!WARNING]
> FrequenC HTTP parser have case-sensitive headers.

## Fetching the version

FrequenC API may change between versions, some clients may want to keep compability with older versions of FrequenC while keeping the same codebase, for that, it's necessary to check the version of FrequenC before connecting to it.

This endpoint is considered stable, and will not change between versions.

```http
GET /version
Authorization: <password>
```

```json
{
  "version": "1.0.0"
}
```

## Decoding track

Clients shouldn't implement the decoding of tracks, as they can rapidly change between versions. Instead, clients should use the `/decodetrack` endpoint. This endpoint is meant for decoding a single track, as it's optimized for that task.

```http
GET /v1/decodetrack?encodedTrack=XXXXXXXXXX
Authorization: <password>
```

```json
{
  "encoded": "XXXXXXXXXX",
  "info": {
    "title": "Title",
    "author": "Author",
    "length": 1000,
    "identifier": "Identifier",
    "isStream": false,
    "uri": "https://example.com",
    "artworkUrl": "https://example.com",
    "isrc": "ISRC",
    "sourceName": "Source",
    "position": 0
  }
}
```

> [!NOTE]
> The `encoded` key is the same as the `encodedTrack` query parameter.

> [!WARNING]
> `uri`, `artworkUrl` and `isrc` may be not be present in all tracks.

## Decoding tracks

Meant for decoding multiple tracks at once, this endpoint is not optimized for decoding a single track, and should not be used for that. Look at [Decoding track](#decoding-track) for that.

```http
POST /v1/decodetracks
Authorization: <password>

[ "XXXXXXXXXX", "XXXXXXXXXX", ... ]
```

```json
[
  { Track Info },
  { Track Info },
  ...
]
```

> [!WARNING]
> `uri`, `artworkUrl` and `isrc` may be not be present in all tracks.

> [!NOTE]
> There isn't a limit on how many tracks can be decoded at once.

## Encoding track

This endpoint is meant for client plugins, allowing them to encode tracks, and customise how they are encoded. This endpoint is optimized for encoding a single track.

```http
POST /v1/encodetrack
Authorization: <password>

{
  Track Info
}
```

```json
[
  "XXXXXXXXXX"
]
```

## Encoding tracks

Meant for encoding multiple tracks at once, this endpoint is not optimized for encoding a single track, and should not be used for that. Look at [Encoding track](#encoding-track) for that.

```http
POST /v1/encodetracks
Authorization: <password>

[
  Track Info,
  Track Info,
  ...
]
```

```json
[
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  ...
]
```

> [!NOTE]
> There isn't a limit on how many tracks can be encoded at once.

## Loading tracks

This endpoint is meant for loading tracks, and it's the main way of loading tracks from certain sources.

```http
POST /v1/loadtracks?identifier=ytsearch:example
Authorization: <password>
```

### Load types

- `search`: A search result.
- `empty`: An empty result.
- `error`: An error result.


<details>

<summary>Search</summary>

```json
{
  "loadType": "search",
  "data": [
    { Track Info },
    { Track Info },
    ...
  ]
}
```

</details>

<details>

<summary>Empty</summary>

```json
{
  "loadType": "empty"
}
```

</details>

<details>

<summary>Error</summary>

```json
{
  "loadType": "error",
  "message": {
    "message": "Error message",
    "severity": "common",
    "cause": "cause"
  }
}
```

</details>

## Connecting to the websocket

The websocket is the main way of communication between the client and FrequenC. It's used for sending events, and receiving commands.

```http
GET /v1/ws
Authorization: <password>
Upgrade: websocket
Connection: Upgrade
User-Id: <user-id>
```

> [!WARNING]
> All headers are required, the leak of any of them will result in a `400 Bad Request` or `401 Unauthorized` response.
