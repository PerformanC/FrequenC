# FrequenC API

The FrequenC API is the main way of communication between the client and the server, and it's used for sending commands, and receiving events.

## Table of Contents

- [FrequenC API](#frequenc-api)
  - [Table of Contents](#table-of-contents)
  - [Used abbreviations](#used-abbreviations)
  - [Development](#development)
  - [/version - Fetching the version](#version---fetching-the-version)
  - [/v1/info - Fetch information about the node](#v1info---fetch-information-about-the-node)
    - [Response body format](#response-body-format)
      - [Structure](#structure)
      - [Example](#example)
  - [/v1/decodetrack - Decoding track](#v1decodetrack---decoding-track)
    - [Request parameters](#request-parameters)
      - [`encodedTrack` - String: The encoded track.](#encodedtrack---string-the-encoded-track)
        - [Structure](#structure)
        - [Example](#example)
    - [Response body format](#response-body-format)
      - [Structure](#structure)
      - [Example](#example)
  - [/v1/decodetracks - Bulk decoding tracks](#v1decodetracks---bulk-decoding-tracks)
    - [Request body format](#request-body-format)
      - [Structure](#structure)
      - [Example](#example)
    - [Response body format](#response-body-format)
      - [Structure](#structure)
      - [Example](#example)
  - [/v1/encodetrack - Encoding track](#v1encodetrack---encoding-track)
    - [Request body format](#request-body-format)
      - [Structure](#structure)
      - [Example](#example)
    - [Response body format](#response-body-format)
      - [Structure](#structure)
      - [Example](#example)
  - [/v1/encodetracks - Bulk encoding tracks](#v1encodetracks---bulk-encoding-tracks)
    - [Request body format](#request-body-format)
      - [Structure](#structure)
      - [Example](#example)
    - [Response body format](#response-body-format)
      - [Structure](#structure)
      - [Example](#example)
  - [/v1/loadtracks - Loading tracks](#v1loadtracks---loading-tracks)
    - [Request parameters](#request-parameters)
      - [`identifier` - String: The identifier of the track.](#identifier---string-the-identifier-of-the-track)
        - [Structure](#structure)
        - [Example](#example)
    - [Response body format](#response-body-format)
      - [Structure](#structure)
      - [Examples](#examples)
  - [/v1/sessions/Xe16/players/Xe18 - Update a player information](#v1sessionsxe16playersxe18---update-a-player-information)
    - [GET](#get)
      - [Response body format](#response-body-format)
        - [Structure](#structure)
        - [Example](#example)
    - [PATCH](#patch)
      - [Request body format](#request-body-format)
        - [Structure](#structure)
        - [Example](#example)
    - [DELETE](#delete)
  - [/v1/websocket - The WebSocket](#v1websocket---the-websocket)
    - [Events](#events)
      - [Ready](#ready)
        - [Structure](#structure)
        - [Example](#example)

> [!IMPORTANT]
> This is a beta version of the API.

> [!NOTE]
> All endpoints require authentication.

> [!WARNING]
> All endpoints, except `/version`, are prefixed with `/v1` and may change between versions.

> [!WARNING]
> We reserve the right of breaking changes without major version bumps if they are said to be necessary in this documentation.

## Used abbreviations

This guide contains examples with strings that are random, but use a fixed length. Those are represented by `Xe` + a number, which is the length of the string.

When a string is not fixed, it will be represented by a random amount of `X` characters, which MUST NOT be taken as a fixed length.

## FrequenC HTTP Parsing

FrequenC uses a custom HTTP parser, which is stricter and faster than most HTTP parsers. Using its customizability, it has been implemented following limitations:

- Maximum headers amount: 10

There is no support for custom `Content-Type`, and all responses should be pure text. However, there is support for `chunked` `Transfer-Encoding`, but should only be used when necessary.

## /version - Fetching the version

Clients may want to support multiple versions of FrequenC, and to make that possible, it's required to create an unversioned endpoint which displays the node version.

- Purpose: Fetches the node version.
- Supported methods: `GET`
- Required headers: `Authorization`
- Response: The version of FrequenC.
- Response status: `200 OK`
- Response type: `text/plain`

## /v1/info - Fetch information about the node

Information about the FrequenC node may be useful for higher-level clients, which may want to disable sources or filters based on the node's capabilities.

- Purpose: Fetches information about the node.
- Supported methods: `GET`
- Required headers: `Authorization`
- Response: Node information JSON
- Response status: `200 OK`
- Response type: `application/json`

### Response body format

#### Structure

- `version` - Object: The version of the node.
  - `major` - Integer: The major version.
  - `minor` - Integer: The minor version.
  - `patch` - Integer: The patch version.
- `git` - Object: Information about the pre-built binary.
  - `branch` - String: The branch of the commit.
  - `commit` - String: The commit SHA-1 hash.
- `source_managers` - Array: The source managers supported by the node.
- `filters` - Array: The filters supported by the node. Always empty in beta.

#### Example

```json
{
  "version": {
    "major": 1,
    "minor": 0,
    "patch": 0
  },
  "git": {
    "branch": "main",
    "commit": "XXXXXXXX"
  },
  "source_managers": [
    "youtube",
    ...
  ],
  "filters": [
    "timescale",
    "volume",
    ...
  ]
}
```

## /v1/decodetrack - Decoding track

Decoding tracks is a core part of the FrequenC design, and it should be used whenever possible over the raw JSON track format.

Clients SHOULD implement local decoding of tracks, as it's faster and more efficient than using the `/decodetrack` endpoint. However, clients MAY use the `/decodetrack` endpoint as a fallback.

- Purpose: Decodes a track.
- Supported methods: `GET`
- Required headers: `Authorization`
- Query parameters: `encodedTrack`
- Response: The track JSON or none.
- Response status: `200 OK` or `400 Bad Request`
- Response type: `application/json` or none.

### Request parameters

#### `encodedTrack` - String: The encoded track.

##### Structure

A [encoded track](ENCODED_TRACK.md) base64-encoded string.

##### Example

```txt
XXXXXXXXXX...
```

### Response body format

#### Structure

- `encoded` - String: The encoded track.
- `info` - Object: The track information.
  - `title` - String: The title of the track.
  - `author` - String: The author of the track.
  - `length` - Number: The length of the track.
  - `identifier` - String: The identifier of the track.
  - `is_stream` - Boolean: If the track is a stream.
  - `uri` - String: The URI of the track.
  - `artwork_url` - Nullable String: The artwork URL of the track.
  - `isrc` - Nullable String: The ISRC of the track.
  - `source_name` - String: The source name of the track.

#### Example

```json
{
  "encoded": "XXXXXXXXXX...",
  "info": {
    "title": "Title",
    "author": "Author",
    "length": 1000,
    "identifier": "Identifier",
    "is_stream": false,
    "uri": "https://example.com",
    "artwork_url": "https://example.com",
    "isrc": "ISRC",
    "source_name": "Source"
  }
}
```

> [!NOTE]
> The `encoded` key is the same as the `encodedTrack` query parameter.

> [!WARNING]
> `artworkUrl` and `isrc` may be not be present in all tracks.

## /v1/decodetracks - Bulk decoding tracks

Decoding multiple tracks at once is a core part of the FrequenC design, and it should be used whenever possible over the raw JSON track format.

Clients SHOULD implement local decoding of tracks, as it's faster and more efficient than using the `/decodetracks` endpoint. However, clients MAY use the `/decodetracks` endpoint as a fallback.

- Purpose: Decodes multiple tracks.
- Supported methods: `POST`
- Required headers: `Authorization`
- Request body: An array of encoded tracks.
- Response: An array of track JSONs or none.
- Response status: `200 OK` or `400 Bad Request`
- Response type: `application/json` or none.

### Request body format

#### Structure

An array of [encoded tracks](ENCODED_TRACK.md) base64-encoded strings.

#### Example

```json
[
  "XXXXXXXXXX...",
  "XXXXXXXXXX...",
  ...
]
```

### Response body format

#### Structure

- `encoded` - String: The encoded track.
- `info` - Object: The track information.
  - `title` - String: The title of the track.
  - `author` - String: The author of the track.
  - `length` - Number: The length of the track.
  - `identifier` - String: The identifier of the track.
  - `is_stream` - Boolean: If the track is a stream.
  - `uri` - String: The URI of the track.
  - `artwork_url` - Nullable String: The artwork URL of the track.
  - `isrc` - Nullable String: The ISRC of the track.
  - `source_name` - String: The source name of the track.

#### Example

```json
[
  {
    "encoded": "XXXXXXXXXX...",
    "info": {
      "title": "Title",
      "author": "Author",
      "length": 1000,
      "identifier": "Identifier",
      "is_stream": false,
      "uri": "https://example.com",
      "artwork_url": "https://example.com",
      "isrc": "ISRC",
      "source_name": "Source"
    }
  },
  {
    "encoded": "XXXXXXXXXX...",
    "info": {
      "title": "Title",
      "author": "Author",
      "length": 1000,
      "identifier": "Identifier",
      "is_stream": false,
      "uri": "https://example.com",
      "artwork_url": "https://example.com",
      "isrc": "ISRC",
      "source_name": "Source"
    }
  },
  ...
]
```

## /v1/encodetrack - Encoding track

Clients SHOULD implement local encoding of tracks, as it's faster and more efficient than using the `/encodetrack` endpoint. However, clients MAY use the `/encodetrack` endpoint as a fallback.

- Purpose: Encodes a track.
- Supported methods: `POST`
- Required headers: `Authorization`
- Request body: The encoded track.
- Response: The encoded track or none.
- Response status: `200 OK` or `400 Bad Request`
- Response type: `application/json` or none.

### Request body format

#### Structure

- `title` - String: The title of the track.
- `author` - String: The author of the track.
- `length` - Number: The length of the track.
- `identifier` - String: The identifier of the track.
- `is_stream` - Boolean: If the track is a stream.
- `uri` - String: The URI of the track.
- `artwork_url` - Nullable String: The artwork URL of the track.
- `isrc` - Nullable String: The ISRC of the track.
- `source_name` - String: The source name of the track.

### Response body format

#### Structure

A [encoded track](ENCODED_TRACK.md) base64-encoded string.

#### Example

```txt
XXXXXXXXXX...
```

## /v1/encodetracks - Bulk encoding tracks

Clients SHOULD implement local encoding of tracks, as it's faster and more efficient than using the `/encodetracks` endpoint. However, clients MAY use the `/encodetracks` endpoint as a fallback.

- Purpose: Encodes multiple tracks.
- Supported methods: `POST`
- Required headers: `Authorization`
- Request body: An array of tracks.
- Response: An array of encoded tracks or none.
- Response status: `200 OK` or `400 Bad Request`
- Response type: `application/json` or none.

### Request body format

#### Structure

- `title` - String: The title of the track.
- `author` - String: The author of the track.
- `length` - Number: The length of the track.
- `identifier` - String: The identifier of the track.
- `is_stream` - Boolean: If the track is a stream.
- `uri` - String: The URI of the track.
- `artwork_url` - Nullable String: The artwork URL of the track.
- `isrc` - Nullable String: The ISRC of the track.
- `source_name` - String: The source name of the track.

#### Example

```json
[
  {
    "title": "Title",
    "author": "Author",
    "length": 1000,
    "identifier": "Identifier",
    "is_stream": false,
    "uri": "https://example.com",
    "artwork_url": "https://example.com",
    "isrc": "ISRC",
    "source_name": "Source"
  },
  {
    "title": "Title",
    "author": "Author",
    "length": 1000,
    "identifier": "Identifier",
    "is_stream": false,
    "uri": "https://example.com",
    "artwork_url": "https://example.com",
    "isrc": "ISRC",
    "source_name": "Source"
  },
  ...
]
```

### Response body format

#### Structure

A [encoded track](ENCODED_TRACK.md) base64-encoded string.

#### Example

```json
[
  "XXXXXXXXXX...",
  "XXXXXXXXXX...",
  ...
]
```

## /v1/loadtracks - Loading tracks

Loading tracks is the most important part of the FrequenC API, and it's used for loading tracks from different sources. It searches the track query in the appropriate source and generates a list of tracks.

- Purpose: Loads tracks from a source.
- Supported methods: `GET`
- Required headers: `Authorization`
- Query parameters: `identifier`
- Response: The load type and data.
- Response status: `200 OK`
- Response type: `application/json`

### Request parameters

#### `identifier` - String: The identifier of the track.

##### Structure

- `prefix` - Non-existable String: The source prefix.
- `query` - String: The query.

> [!NOTE]
> The `prefix` is not required, and it's only used for identifying the source when performing a search. Currently, only YouTube (`ytsearch:`) is supported.

##### Example

```txt
ytsearch:example
```

### Response body format

#### Structure

- `loadType` - String: The load type.
- `data` - Non-existable Array, Object or String: The track information.

#### Examples

##### Search

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

##### Empty

```json
{
  "loadType": "empty"
}
```

##### Error

```json
{
  "loadType": "error",
  "data": "Error message"
}
```

## /v1/sessions/Xe16/players/Xe18 - Update a player information

This endpoint allows clients to update information about the player, allowing FrequenC to connect to the Discord voice servers, update player state and etc.

- Purpose: Updates player information.
- Supported methods: `GET`, `PATCH` and `DELETE`
- Required headers: `Authorization`
- Request body: The player information.
- Response: None.
- Response status: `204 No Content`
- Response type: None.

### GET

#### Response body format

##### Structure

- `voice` - Non-existable Object: The voice information.
  - `endpoint` - String: The Discord voice server endpoint.
  - `session_id` - String: The Discord voice server session ID.
  - `token` - String: The Discord voice server token.

##### Example

```json
{
  "voice": {
    "endpoint": "brazil1001.discord.media",
    "session_id": "Xe32",
    "token": "Xe16"
  }
}
```

### PATCH

#### Request body format

##### Structure

- `voice` - Non-existable Object: The voice information.
  - `endpoint` - String: The Discord voice server endpoint.
  - `session_id` - String: The Discord voice server session ID.
  - `token` - String: The Discord voice server token.

> [!WARNING]
> The `endpoint`, `session_id` and `token` are Discord voice server specific, and should not be shared with anyone. However they're only temporarily confidential.

##### Example

```json
{
  "voice": {
    "endpoint": "brazil1001.discord.media",
    "session_id": "Xe32",
    "token": "Xe16"
  }
}
```

### DELETE

Returns a `204 No Content` response.

## /v1/websocket - The WebSocket

The websocket is the main way of communication between the client and FrequenC. It's used for  receiving numerous events of the node and players.

- Purpose: Connects to the WebSocket.
- Supported methods: `GET`
- Required headers: `Authorization`, `User-Id` and `Client-Info` (`NAME/VERSION (BOT NAME)`)
- Response: None.

> [!NOTE]
> Resuming support is expected, but not confirmed.

### Events

FrequenC uses events sent in JSON encoding, allowing clients to easily understand by using a JSON parser.

- `op` - String: The event op.

#### Ready

##### Structure

- `op` - String: The event op. (ready)
- `resumed` - Boolean: If the session was resumed. Clients SHOULD NOT parse this as of beta.
- `session_id` - String: The session ID.

##### Example

```json
{
  "op": "ready",
  "resumed": false,
  "session_id": "Xe16"
}
```

