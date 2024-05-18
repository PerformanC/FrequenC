# FrequenC API Encoded Track

The FrequenC API uses a track format, used to pass track information in a compact way. This document will document the track format, allowing easy decoding and encoding of tracks.

## Table of Contents

- [FrequenC API Encoded Track](#frequenc-api-encoded-track)
  
> [!IMPORTANT]
> This is a beta version of the API.

## Encoded format

The encoded track format is a modified version of LavaLink track format, modified to have a better design, keeping it even more compact. It is a base64-encoded buffer, containing the following information:

| Name               | Value type          | Description                      |
| ------------------ | ------------------- | -------------------------------- |
| Length             | Unsigned int 32-bit | The length of the buffer.        |
| Version            | Byte                | The version of the encoded. (1)  |
| Title length       | Unsigned int 16-bit | The length of the title.         |
| Title              | Char                | The title of the track.          |
| Author length      | Unsigned int 16-bit | The length of the author.        |
| Author             | Char                | The author of the track.         |
| Length             | Unsigned int 32-bit | The length of the track.         |
| Identifier         | Char                | The identifier of the track.     |
| Is stream          | Byte (boolean)      | If the track is a stream.        |
| URI length         | Unsigned int 16-bit | The length of the URI.           |
| URI                | Char                | The URI of the track.            |
| Artwork URL        | Byte (boolean)      | If the track has an artwork URL. |
| Artwork URL        | Char                | The artwork URL of the track.    |
| ISRC               | Byte (boolean)      | If the track has an ISRC.        |
| ISRC               | Char                | The ISRC of the track.           |
| Source name length | Unsigned int 16-bit | The length of the source name.   |
| Source name        | Char                | The source name of the track.    |

The fields must strictly follow the order and length, or else FrequenC will not be able to decode the track and will reject the client's request.

> [!NOTE]
> Clients MUST only decode the track if the version is 1. If the version is different, the client MUST reject the track.
