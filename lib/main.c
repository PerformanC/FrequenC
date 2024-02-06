#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "httpserver.h"
#include "websocket.h"
#define JSMN_STRICT /* Set strict mode for jsmn (JSON tokenizer) */
#include "jsmn.h"
#include "jsmn-find.h"
#include "utils.h"
#include "tablec.h"
#include "queryparser.h"
#include "track.h"
#include "urldecode.h"
#include "cthreads.h"
#include "csocket-server.h"
#include "types.h"

/* Sources */
#include "youtube.h"
/* Sources end */

#define VERSION "2.0.0"
#define VERSION_LENGTH "5"
#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define GIT_BRANCH "main"
#define GIT_COMMIT "unknown"
#define GIT_COMMIT_TIME "-1"

#define SUPPORTED_SOURCES "[]"
#define SUPPORTED_FILTERS "[]"

#ifndef AUTHORIZATION
#define AUTHORIZATION "youshallnotpass"
#endif

struct ClientAuthorization {
  char *userId;
  struct csocket_server_client *client;
  bool disconnected;
};

struct tablec_ht clients;
struct httpserver server;
struct cthreads_mutex mutex;

void callback(struct csocket_server_client *client, int socketIndex, struct httpparser_request *request) {
  struct httpparser_header *authorization = httpparser_get_header(request, "Authorization");

  if (authorization == NULL || strcmp(authorization->value, AUTHORIZATION) != 0) {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 401);
    setResponseHeader(&response, "Content-Type", "text/plain");
    setResponseHeader(&response, "Content-Length", "12");
    setResponseBody(&response, "Unauthorized");

    sendResponse(&response);

    return;
  }

  if (strcmp(request->path, "/v1/websocket") == 0) {
    struct httpparser_header *header = httpparser_get_header(request, "Upgrade");
    if (header == NULL || strcmp(header->value, "websocket") != 0) goto bad_request;

    struct httpparser_header *secWebSocketKey = httpparser_get_header(request, "Sec-WebSocket-Key");
    if (secWebSocketKey == NULL) goto bad_request;

    struct httpparser_header *userId = httpparser_get_header(request, "User-Id");
    if (userId == NULL) goto bad_request;

    struct httpparser_header *UserAgent = httpparser_get_header(request, "Client-Name");
    if (UserAgent == NULL) goto bad_request;

    printf("[main]: %s client connected to FrequenC.\n", UserAgent->value);

    struct httpparser_header *sessionId = httpparser_get_header(request, "Session-Id");

    struct httpserver_response response;
    struct httpserver_header headerBuffer[5];
    initResponse(&response, headerBuffer, 5);

    char acceptKey[32 + 1];
    frequenc_gen_accept_key(secWebSocketKey->value, acceptKey);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 101);
    setResponseHeader(&response, "Upgrade", "websocket");
    setResponseHeader(&response, "Connection", "Upgrade");
    setResponseHeader(&response, "Sec-WebSocket-Accept", acceptKey);
    setResponseHeader(&response, "Sec-WebSocket-Version", "13");

    sendResponse(&response);

    struct tablec_bucket *selectedClient;

    cthreads_mutex_lock(&mutex);
    if (sessionId != NULL && (selectedClient = tablec_get(&clients, sessionId->value)) != NULL && ((struct ClientAuthorization *)selectedClient->value)->disconnected) {
      cthreads_mutex_unlock(&mutex);

      struct ClientAuthorization *clientAuthorization = selectedClient->value;

      clientAuthorization->userId = userId->value;
      clientAuthorization->client = client;

      char payload[1024];
      int payloadLength = snprintf(payload, sizeof(payload),
        "{"
          "\"op\":\"ready\","
          "\"resumed\":true,"
          "\"sessionId\":\"%s\""
        "}", sessionId->value
      );

      struct frequenc_ws_message wsResponse;
      wsResponse.opcode = 1;
      wsResponse.fin = 1;
      wsResponse.payloadLength = payloadLength;
      wsResponse.buffer = payload;
      wsResponse.client = client;

      frequenc_send_ws_response(&wsResponse);

      setSocketData(&server, socketIndex, sessionId->value);

      upgradeSocket(&server, socketIndex);

      return;
    }

    cthreads_mutex_unlock(&mutex);

    struct ClientAuthorization *clientAuthorization = frequenc_safe_malloc(sizeof(struct ClientAuthorization));

    char *sessionIdGen = frequenc_safe_malloc((16 + 1) * sizeof(char));

    generateSession:
      frequenc_generate_session_id(sessionIdGen);

      if (tablec_get(&clients, sessionIdGen) != NULL) {
        printf("[main]: Session ID collision detected. Generating another one.\n");

        goto generateSession;
      }
    
    clientAuthorization->userId = userId->value;
    clientAuthorization->client = client;
    clientAuthorization->disconnected = false;

    char payload[1024];
    int payloadLength = snprintf(payload, sizeof(payload),
      "{"
        "\"op\":\"ready\","
        "\"resumed\":false,"
        "\"sessionId\":\"%s\""
      "}", sessionIdGen
    );

    struct frequenc_ws_message wsResponse;
    wsResponse.opcode = 1;
    wsResponse.fin = 1;
    wsResponse.payloadLength = payloadLength;
    wsResponse.buffer = payload;
    wsResponse.client = client;

    frequenc_send_ws_response(&wsResponse);

    cthreads_mutex_lock(&mutex);
    if (tablec_set(&clients, sessionIdGen, clientAuthorization) == -1) {
      cthreads_mutex_unlock(&mutex);
      printf("[main]: Hashtable is full, resizing.\n");

      struct tablec_ht oldClients = clients;
      size_t newCapacity = clients.capacity * 2;
      struct tablec_bucket *newBuckets = frequenc_safe_malloc(newCapacity * sizeof(struct tablec_bucket));

      cthreads_mutex_lock(&mutex);
      if (tablec_resize(&clients, newBuckets, newCapacity) == -1) {
        printf("[main]: Hashtable resizing failed. Exiting.\n");

        exit(1);
      }
      cthreads_mutex_unlock(&mutex);

      free(oldClients.buckets);

      cthreads_mutex_lock(&mutex);
      tablec_set(&clients, sessionIdGen, clientAuthorization);
    }
    cthreads_mutex_unlock(&mutex);

    setSocketData(&server, socketIndex, sessionIdGen);

    upgradeSocket(&server, socketIndex);
  }

  else if (strcmp(request->path, "/version") == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "text/plain");
    setResponseHeader(&response, "Content-Length", VERSION_LENGTH);
    setResponseBody(&response, VERSION);

    sendResponse(&response);
  }

  else if (strcmp(request->path, "/v1/info") == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    char payload[1024];
    char payloadLength[4];
    frequenc_stringify_int(snprintf(payload, sizeof(payload),
      "{"
        "\"version\":{"
          "\"semver\":\"%s\","
          "\"major\":%d,"
          "\"minor\":%d,"
          "\"patch\":%d,"
          "\"preRelease\":null"
        "},"
        "\"builtTime\":-1,"
        "\"git\":{"
          "\"branch\":\"%s\","
          "\"commit\":\"%s\","
          "\"commitTime\":%s"
        "},"
        "\"isFrequenC\":true,"
        "\"sourceManagers\":%s,"
        "\"filters\":%s,"
        "\"plugins\":[]"
      "}",
      VERSION, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, GIT_BRANCH, GIT_COMMIT, GIT_COMMIT_TIME, SUPPORTED_SOURCES, SUPPORTED_FILTERS),
      payloadLength
    );

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "application/json");
    setResponseHeader(&response, "Content-Length", payloadLength);
    setResponseBody(&response, payload);

    sendResponse(&response);
  }

  else if (strcmp(request->path, "/v1/decodetracks") == 0) {
    if (strcmp(request->method, "POST") != 0) goto method_not_allowed;

    if (!request->body) {
      printf("[main]: No body found.\n");

      goto bad_request;
    }

    jsmn_parser parser;
    jsmntok_t tokens[256];

    struct httpparser_header *contentLength = httpparser_get_header(request, "Content-Length");

    jsmn_init(&parser);
    int r = jsmn_parse(&parser, request->body, atoi(contentLength->value), tokens, 256);
    if (r < 0) {
      printf("[main]: Failed to parse JSON: %d\n", r);

      goto bad_request;
    }

    jsmnf_loader loader;
    jsmnf_pair pairs[256];

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, request->body, tokens, parser.toknext, pairs, 256);
    if (r < 0) {
      printf("[main]: Failed to load JSON: %d\n", r);

      goto bad_request;
    }

    char *arrayResponse = frequenc_safe_malloc(2048 * sizeof(char));
    size_t arrayResponseLength = 1;
    arrayResponse[0] = '[';

    jsmnf_pair *f;

    for (int i = 0; i < pairs->size; i++) {
      f = &pairs->fields[i];

      char encodedTrack[512];
      snprintf(encodedTrack, sizeof(encodedTrack), "%.*s", (int)f->v.len, request->body + f->v.pos);

      struct frequenc_track_info decodedTrack = { 0 };
      int status = decodeTrack(&decodedTrack, encodedTrack);

      if (status == -1) goto bad_request;

      char payload[2048];
      frequenc_partial_track_info_to_json(&decodedTrack, payload, sizeof(payload));

      tstr_append(arrayResponse, payload, &arrayResponseLength, 0);

      if (i != pairs->size - 1)
        tstr_append(arrayResponse, ",", &arrayResponseLength, 0);

      frequenc_free_track_info(&decodedTrack);
    }

    arrayResponse[arrayResponseLength] = ']';
    arrayResponseLength++;
    arrayResponse[arrayResponseLength] = '\0';

    char payloadLength[8 + 1];
    frequenc_stringify_int(arrayResponseLength, payloadLength);

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "application/json");
    setResponseHeader(&response, "Content-Length", payloadLength);
    setResponseBody(&response, arrayResponse);
    
    sendResponse(&response);

    free(arrayResponse);
  }

  else if (strncmp(request->path, "/v1/decodetrack", sizeof("/v1/decodetrack") - 1) == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    struct qparser_query queries[2];
    struct qparser_info parseInfo;

    qparser_init(&parseInfo, queries, 2);
    qparser_parse(&parseInfo, request->path);

    struct qparser_query *query = qparser_get_query(&parseInfo, "encodedTrack");

    if (query == NULL || query->value[0] == '\0') goto bad_request;

    char decodedQuery[512];
    urldecoder_decode(decodedQuery, query->value);

    struct frequenc_track_info decodedTrack;
    int status = decodeTrack(&decodedTrack, decodedQuery);

    if (status == -1) goto bad_request;

    char payload[2048];
    char payloadLength[3 + 1];
    frequenc_stringify_int(frequenc_track_info_to_json(&decodedTrack, decodedQuery, payload, sizeof(payload)), payloadLength);

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "application/json");
    setResponseHeader(&response, "Content-Length", payloadLength);
    setResponseBody(&response, payload);

    sendResponse(&response);

    freeTrack(&decodedTrack);
  }

  else if (strcmp(request->path, "/v1/encodetracks") == 0) {
    if (strcmp(request->method, "POST") != 0) goto method_not_allowed;

    if (!request->body) goto bad_request;

    jsmn_parser parser;
    jsmntok_t tokens[256];

    struct httpparser_header *contentLength = httpparser_get_header(request, "Content-Length");

    jsmn_init(&parser);
    int r = jsmn_parse(&parser, request->body, atoi(contentLength->value), tokens, 256);
    if (r < 0) {
      printf("[main]: Failed to parse JSON: %d\n", r);

      goto bad_request;
    }

    jsmnf_loader loader;
    jsmnf_pair pairs[256];

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, request->body, tokens, parser.toknext, pairs, 256);
    if (r < 0) {
      printf("[main]: Failed to load JSON: %d\n", r);

      goto bad_request;
    }

    char *arrayResponse = frequenc_safe_malloc((1024 * pairs->size) * sizeof(char));
    size_t arrayResponseLength = 1;
    arrayResponse[0] = '[';

    for (int i = 0; i < pairs->size; i++) {
      char iStr[3 + 1];
      snprintf(iStr, sizeof(iStr), "%d", i);

      struct frequenc_track_info decodedTrack = { 0 };
      char *path[2] = { iStr };
      if (frequenc_json_to_track_info(&decodedTrack, pairs, request->body, path, 1, sizeof(path) / sizeof(*path)) == -1) {
        frequenc_free_track_info(&decodedTrack);
        free(arrayResponse);

        goto bad_request;
      }

      char *encodedTrack = NULL;
      int status = encodeTrack(&decodedTrack, &encodedTrack);

      if (status == -1) {
        frequenc_free_track_info(&decodedTrack);
        free(arrayResponse);

        goto bad_request;
      }

      arrayResponse[arrayResponseLength] = '"';
      arrayResponseLength++;

      tstr_append(arrayResponse, encodedTrack, &arrayResponseLength, 0);

      if (i != pairs->size - 1) {
        tstr_append(arrayResponse, "\",", &arrayResponseLength, 0);
      } else {
        tstr_append(arrayResponse, "\"", &arrayResponseLength, 0);
      }

      frequenc_free_track_info(&decodedTrack);
      free(encodedTrack);
    }

    tstr_append(arrayResponse, "]", &arrayResponseLength, 0);

    char payloadLength[8 + 1];
    frequenc_stringify_int(arrayResponseLength, payloadLength);

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "application/json");
    setResponseHeader(&response, "Content-Length", payloadLength);
    setResponseBody(&response, arrayResponse);

    sendResponse(&response);

    free(arrayResponse);
  }

  else if (strcmp(request->path, "/v1/encodetrack") == 0) {
    if (strcmp(request->method, "POST") != 0) goto method_not_allowed;

    jsmn_parser parser;
    jsmntok_t tokens[32];

    struct httpparser_header *contentLength = httpparser_get_header(request, "Content-Length");

    jsmn_init(&parser);
    int r = jsmn_parse(&parser, request->body, atoi(contentLength->value), tokens, 32);
    if (r < 0) {
      printf("[main]: Failed to parse JSON: %d\n", r);

      goto bad_request;
    }

    jsmnf_loader loader;
    jsmnf_pair pairs[32];

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, request->body, tokens, parser.toknext, pairs, 32);
    if (r < 0) {
      printf("[main]: Failed to load JSON: %d\n", r);

      goto bad_request;
    }

    struct frequenc_track_info decodedTrack = { 0 };
    char *path[1] = { 0 };
    if (frequenc_json_to_track_info(&decodedTrack, pairs, request->body, path, 0, sizeof(path) / sizeof(*path)) == -1) {
      frequenc_free_track_info(&decodedTrack);

      goto bad_request;
    }

    char *encodedTrack = NULL;
    char payloadLength[4];
    frequenc_stringify_int(encodeTrack(&decodedTrack, &encodedTrack), payloadLength);

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "text/plain");
    setResponseHeader(&response, "Content-Length", payloadLength);
    setResponseBody(&response, encodedTrack);
    
    sendResponse(&response);

    frequenc_free_track_info(&decodedTrack);
    free(encodedTrack);
  }

  /* todo: identify bottleneck while doing loadtracks, possibly in httpclient */
  else if (strncmp(request->path, "/v1/loadtracks", sizeof("/v1/loadtracks") - 1) == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    struct qparser_query queries[2];
    struct qparser_info parseInfo;

    qparser_init(&parseInfo, queries, 2);
    qparser_parse(&parseInfo, request->path);

    struct qparser_query *identifier = qparser_get_query(&parseInfo, "identifier");

    if (identifier == NULL || identifier->value[0] == '\0') goto bad_request;

    char identifierDecoded[512];
    urldecoder_decode(identifierDecoded, identifier->value);

    struct tstr_string result = { 0 };

    if (strncmp(identifierDecoded, "ytsearch:", sizeof("ytsearch:") - 1) == 0) {
      result = frequenc_search_youtube(identifierDecoded + sizeof("ytsearch:") - 1, 0);
    }

    char payloadLength[8 + 1];
    frequenc_stringify_int(result.length, payloadLength);

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 200);
    setResponseHeader(&response, "Content-Type", "application/json");
    setResponseHeader(&response, "Content-Length", payloadLength);
    setResponseBody(&response, result.string);

    sendResponse(&response);

    if (result.allocated)
      free(result.string);
  }

  else {
    struct httpserver_response notFoundResponse;
    struct httpserver_header headerBuffer[3];
    initResponse(&notFoundResponse, headerBuffer, 2);

    setResponseSocket(&notFoundResponse, client);
    setResponseStatus(&notFoundResponse, 404);

    sendResponse(&notFoundResponse);
  }

  return;

  bad_request: {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 400);

    sendResponse(&response);

    return;
  }

  method_not_allowed: {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    initResponse(&response, headerBuffer, 3);

    setResponseSocket(&response, client);
    setResponseStatus(&response, 405);

    sendResponse(&response);

    return;
  }
}

int websocketCallback(struct csocket_server_client *client, struct frequenc_ws_header *frameHeader) {
  printf("[main]: WS message: %d\n", csocket_server_client_get_id(client));

  if (frameHeader->opcode == 8) {
    disconnectClient(client);

    return 1;
  }

  printf("[main]: Socket: %d\n", csocket_server_client_get_id(client));
  printf("[main]: Opcode: %d\n", frameHeader->opcode);
  printf("[main]: FIN: %d\n", frameHeader->fin);
  printf("[main]: Length: %zu\n", frameHeader->payloadLength);
  printf("[main]: Buffer: |%s|\n", frameHeader->buffer);

  return 0;
}

void disconnectCallback(struct csocket_server_client *client, int socketIndex) {
  /* TODO: Add resuming mechanism; set clientAuthorization->disconnected to true */
  char socketStr[4];
  snprintf(socketStr, sizeof(socketStr), "%d", csocket_server_client_get_id(client));

  char *sessionId = getSocketData(&server, socketIndex);

  if (sessionId == NULL) return;

  cthreads_mutex_lock(&mutex);
  struct tablec_bucket *bucket2 = tablec_get(&clients, sessionId);

  if (bucket2->value == NULL) {
    printf("[main]: Client disconnected not properly. Report it.\n - Reason: Can't find saved sessionId on hashtable.\n - Socket: %d\n - Socket index: %d\n - Thread ID: %lu\n", csocket_server_client_get_id(client), socketIndex, cthreads_thread_id(cthreads_thread_self()));

    exit(1);

    return;
  }

  tablec_del(&clients, bucket2->key);
  cthreads_mutex_unlock(&mutex);

  free(bucket2->value);
  free(sessionId);

  return;
}

void handleSigInt(int sig) {
  (void) sig;

  printf("\n[main]: Stopping server. Checking the hashtable...\n");

  bool detected = false;
  for (size_t i = 0; i < clients.capacity; i++) {
    struct tablec_bucket bucket = clients.buckets[i];

    if (bucket.key == NULL) continue;

    printf("[main]: Found data in the hashtable.\n - Key: %s\n - Value: %p\n", bucket.key, bucket.value);
  }

  if (!detected) printf("[main]: No data found in the hashtable.\n");
  else printf("[main]: Data found in the hashtable. If there weren't clients connected, please, contribute to the project\n");

  stopServer(&server);
  free(clients.buckets);

  printf("[main]: Checking done. Server stopped. Goodbye.\n");
}

int main(void) {
  #if !__linux__ && !_WIN32 && ALLOW_UNSECURE_RANDOM
    printf("[main]: Warning! Using unsecure random seed. If your system supports /dev/urandom, disable ALLOW_UNSECURE_RANDOM and compile again.\n");
  #endif

  signal(SIGINT, handleSigInt);
  struct tablec_bucket *buckets = frequenc_safe_malloc(sizeof(struct tablec_bucket) * 100);
  tablec_init(&clients, buckets, 100);

  cthreads_mutex_init(&mutex, NULL);

  startServer(&server);

  handleRequest(&server, callback, websocketCallback, disconnectCallback);
}
