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

#define VERSION "1.0.0"
#define VERSION_LENGTH "5"
#define VERSION_MAJOR 1
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

struct client_authorization {
  char *userId;
  struct csocket_server_client *client;
  bool disconnected;
};

struct tablec_ht clients;
struct httpserver server;
struct cthreads_mutex mutex;

void callback(struct csocket_server_client *client, int socket_index, struct httpparser_request *request) {
  struct httpparser_header *authorization = httpparser_get_header(request, "Authorization");

  if (authorization == NULL || strcmp(authorization->value, AUTHORIZATION) != 0) {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 401);
    httpserver_set_response_header(&response, "Content-Type", "text/plain");
    httpserver_set_response_header(&response, "Content-Length", "12");
    httpserver_set_response_body(&response, "Unauthorized");

    httpserver_send_response(&response);

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

    /* todo: add optional Bot-Name header (?) */

    struct httpserver_response response;
    struct httpserver_header headerBuffer[5];
    httpserver_init_response(&response, headerBuffer, 5);

    char acceptKey[32 + 1];
    frequenc_gen_accept_key(secWebSocketKey->value, acceptKey);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 101);
    httpserver_set_response_header(&response, "Upgrade", "websocket");
    httpserver_set_response_header(&response, "Connection", "Upgrade");
    httpserver_set_response_header(&response, "Sec-WebSocket-Accept", acceptKey);
    httpserver_set_response_header(&response, "Sec-WebSocket-Version", "13");

    httpserver_send_response(&response);

    struct tablec_bucket *selectedClient;

    cthreads_mutex_lock(&mutex);
    if (sessionId != NULL && (selectedClient = tablec_get(&clients, sessionId->value)) != NULL && ((struct client_authorization *)selectedClient->value)->disconnected) {
      cthreads_mutex_unlock(&mutex);

      struct client_authorization *client_auth = selectedClient->value;

      client_auth->userId = userId->value;
      client_auth->client = client;

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
      wsResponse.payload_length = payloadLength;
      wsResponse.buffer = payload;
      wsResponse.client = client;

      frequenc_send_ws_response(&wsResponse);

      httpserver_set_socket_data(&server, socket_index, sessionId->value);

      httpserver_upgrade_socket(&server, socket_index);

      return;
    }

    cthreads_mutex_unlock(&mutex);

    struct client_authorization *client_auth = frequenc_safe_malloc(sizeof(struct client_authorization));

    char *sessionIdGen = frequenc_safe_malloc((16 + 1) * sizeof(char));

    generateSession:
      frequenc_generate_session_id(sessionIdGen);

      if (tablec_get(&clients, sessionIdGen) != NULL) {
        printf("[main]: Session ID collision detected. Generating another one.\n");

        goto generateSession;
      }
    
    client_auth->userId = userId->value;
    client_auth->client = client;
    client_auth->disconnected = false;

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
    wsResponse.payload_length = payloadLength;
    wsResponse.buffer = payload;
    wsResponse.client = client;

    frequenc_send_ws_response(&wsResponse);

    cthreads_mutex_lock(&mutex);
    if (tablec_set(&clients, sessionIdGen, client_auth) == -1) {
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
      tablec_set(&clients, sessionIdGen, client_auth);
    }
    cthreads_mutex_unlock(&mutex);

    httpserver_set_socket_data(&server, socket_index, sessionIdGen);

    httpserver_upgrade_socket(&server, socket_index);
  }

  else if (strcmp(request->path, "/version") == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "text/plain");
    httpserver_set_response_header(&response, "Content-Length", VERSION_LENGTH);
    httpserver_set_response_body(&response, VERSION);

    httpserver_send_response(&response);
  }

  else if (strcmp(request->path, "/v1/info") == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    char payload[1024];
    char payloadLength[4];
    frequenc_stringify_int(snprintf(payload, sizeof(payload),
      "{"
        "\"version\":{"
          "\"major\":%d,"
          "\"minor\":%d,"
          "\"patch\":%d"
        "},"
        "\"builtTime\":-1,"
        "\"git\":{"
          "\"branch\":\"%s\","
          "\"commit\":\"%s\","
          "\"commitTime\":%s"
        "},"
        "\"sourceManagers\":%s,"
        "\"filters\":%s"
      "}",
      VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, GIT_BRANCH, GIT_COMMIT, GIT_COMMIT_TIME, SUPPORTED_SOURCES, SUPPORTED_FILTERS),
      payloadLength,
      sizeof(payloadLength)
    );

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payloadLength);
    httpserver_set_response_body(&response, payload);

    httpserver_send_response(&response);
  }

  else if (strcmp(request->path, "/v1/decodetracks") == 0) {
    if (strcmp(request->method, "POST") != 0) goto method_not_allowed;

    if (!request->body) {
      printf("[main]: No body found.\n");

      goto bad_request;
    }

    struct httpparser_header *contentLength = httpparser_get_header(request, "Content-Length");

    jsmn_parser parser;
    jsmntok_t *tokens = NULL;
    unsigned num_tokens = 0;

    jsmn_init(&parser);
    int r = jsmn_parse_auto(&parser, request->body, atoi(contentLength->value), &tokens, &num_tokens);
    if (r <= 0) {
      free(tokens);

      printf("[main]: Failed to parse JSON: %d\n", r);

      goto bad_request;
    }

    jsmnf_loader loader;
    jsmnf_pair *pairs = NULL;
    unsigned num_pairs = 0;

    jsmnf_init(&loader);
    r = jsmnf_load_auto(&loader, request->body, tokens, num_tokens, &pairs, &num_pairs);
    if (r <= 0) {
      free(tokens);
      free(pairs);
      
      printf("[main]: Failed to load JSON: %d\n", r);

      goto bad_request;
    }

    char *arrayResponse = frequenc_safe_malloc(2048 * sizeof(char));
    size_t arrayResponseLength = 1;
    arrayResponse[0] = '[';

    jsmnf_pair *f;

    for (int i = 0; i < pairs->size; i++) {
      f = &pairs->fields[i];

      char encoded_track[512];
      snprintf(encoded_track, sizeof(encoded_track), "%.*s", (int)f->v.len, request->body + f->v.pos);

      struct frequenc_track_info decoded_track = { 0 };
      int status = frequenc_decode_track(&decoded_track, encoded_track);

      if (status == -1) goto bad_request;

      char payload[2048];
      frequenc_partial_track_info_to_json(&decoded_track, payload, sizeof(payload));

      tstr_append(arrayResponse, payload, &arrayResponseLength, 0);

      if (i != pairs->size - 1)
        tstr_append(arrayResponse, ",", &arrayResponseLength, 0);

      frequenc_free_track_info(&decoded_track);
    }

    arrayResponse[arrayResponseLength] = ']';
    arrayResponseLength++;
    arrayResponse[arrayResponseLength] = '\0';

    char payloadLength[8 + 1];
    frequenc_stringify_int(arrayResponseLength, payloadLength, sizeof(payloadLength));

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payloadLength);
    httpserver_set_response_body(&response, arrayResponse);
    
    httpserver_send_response(&response);

    free(arrayResponse);
    free(pairs);
    free(tokens);
  }

  else if (strncmp(request->path, "/v1/decodetrack", sizeof("/v1/decodetrack") - 1) == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    struct qparser_query queries[2];
    struct qparser_info parseInfo;

    qparser_init(&parseInfo, queries, 2);
    qparser_parse(&parseInfo, request->path);

    struct qparser_query *query = qparser_get_query(&parseInfo, "encodedTrack");

    if (query == NULL || query->value[0] == '\0') goto bad_request;

    char *decoded_query = frequenc_safe_malloc((urldecoder_decode_length(query->value) + 1) * sizeof(char));
    urldecoder_decode(decoded_query, query->value);

    struct frequenc_track_info decoded_track;
    int status = frequenc_decode_track(&decoded_track, decoded_query);

    if (status == -1) {
      free(decoded_query);

      goto bad_request;
    }

    char payload[2048];
    char payloadLength[3 + 1];
    frequenc_stringify_int(frequenc_track_info_to_json(&decoded_track, decoded_query, payload, sizeof(payload)), payloadLength, sizeof(payloadLength));

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payloadLength);
    httpserver_set_response_body(&response, payload);

    httpserver_send_response(&response);

    frequenc_free_track(&decoded_track);
    free(decoded_query);
  }

  else if (strcmp(request->path, "/v1/encodetracks") == 0) {
    if (strcmp(request->method, "POST") != 0) goto method_not_allowed;

    if (!request->body) {
      printf("[main]: No body found.\n");

      goto bad_request;
    }

    struct httpparser_header *contentLength = httpparser_get_header(request, "Content-Length");

    jsmn_parser parser;
    jsmntok_t *tokens = NULL;
    unsigned num_tokens = 0;

    jsmn_init(&parser);
    int r = jsmn_parse_auto(&parser, request->body, atoi(contentLength->value), &tokens, &num_tokens);
    if (r <= 0) {
      free(tokens);

      printf("[main]: Failed to parse JSON: %d\n", r);

      goto bad_request;
    }

    jsmnf_loader loader;
    jsmnf_pair *pairs = NULL;
    unsigned num_pairs = 0;

    jsmnf_init(&loader);
    r = jsmnf_load_auto(&loader, request->body, tokens, num_tokens, &pairs, &num_pairs);
    if (r <= 0) {
      free(tokens);
      free(pairs);
      
      printf("[main]: Failed to load JSON: %d\n", r);

      goto bad_request;
    }

    if (pairs->size == 0) goto bad_request;

    char *arrayResponse = frequenc_safe_malloc((2048 * pairs->size) * sizeof(char));
    size_t arrayResponseLength = 1;
    arrayResponse[0] = '[';

    for (int i = 0; i < pairs->size; i++) {
      char i_str[10 + 1];
      snprintf(i_str, sizeof(i_str), "%d", i);

      struct frequenc_track_info decoded_track = { 0 };
      char *path[2] = { i_str };
      if (frequenc_json_to_track_info(&decoded_track, pairs, request->body, path, 1, sizeof(path) / sizeof(*path)) == -1) {
        frequenc_free_track_info(&decoded_track);
        free(arrayResponse);

        goto bad_request;
      }

      char *encoded_track = NULL;
      int status = frequenc_encode_track(&decoded_track, &encoded_track);

      if (status == -1) {
        frequenc_free_track_info(&decoded_track);
        free(arrayResponse);
        free(pairs);
        free(tokens);

        goto bad_request;
      }

      arrayResponse[arrayResponseLength] = '"';
      arrayResponseLength++;

      tstr_append(arrayResponse, encoded_track, &arrayResponseLength, 0);

      if (i != pairs->size - 1) {
        tstr_append(arrayResponse, "\",", &arrayResponseLength, 0);
      } else {
        tstr_append(arrayResponse, "\"", &arrayResponseLength, 0);
      }

      frequenc_free_track_info(&decoded_track);
      free(encoded_track);
    }

    tstr_append(arrayResponse, "]", &arrayResponseLength, 0);

    char payloadLength[8 + 1];
    frequenc_stringify_int(arrayResponseLength, payloadLength, sizeof(payloadLength));

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payloadLength);
    httpserver_set_response_body(&response, arrayResponse);

    httpserver_send_response(&response);

    free(arrayResponse);
    free(pairs);
    free(tokens);
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

    struct frequenc_track_info decoded_track = { 0 };
    char *path[1] = { 0 };
    if (frequenc_json_to_track_info(&decoded_track, pairs, request->body, path, 0, sizeof(path) / sizeof(*path)) == -1) {
      frequenc_free_track_info(&decoded_track);

      goto bad_request;
    }

    char *encoded_track = NULL;
    char payloadLength[4];
    frequenc_stringify_int(frequenc_encode_track(&decoded_track, &encoded_track), payloadLength, sizeof(payloadLength));

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "text/plain");
    httpserver_set_response_header(&response, "Content-Length", payloadLength);
    httpserver_set_response_body(&response, encoded_track);
    
    httpserver_send_response(&response);

    frequenc_free_track_info(&decoded_track);
    free(encoded_track);
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

    if (strncmp(identifierDecoded, "ytsearch:", sizeof("ytsearch:") - 1) == 0)
      result = frequenc_youtube_search(identifierDecoded + (sizeof("ytsearch:") - 1), 0);

    /* add ytm support*/
    // if (strncmp(identifierDecoded, "ytmsearch:", sizeof("ytmsearch:") - 1) == 0)
    //   result = frequenc_youtube_search(identifierDecoded + (sizeof("ytmsearch:") - 1), 1);

    char payloadLength[8 + 1];
    frequenc_stringify_int(result.length, payloadLength, sizeof(payloadLength));

    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payloadLength);
    httpserver_set_response_body(&response, result.string);

    httpserver_send_response(&response);

    if (result.allocated)
      free(result.string);
  }

  else {
    struct httpserver_response notFoundResponse;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&notFoundResponse, headerBuffer, 2);

    httpserver_set_response_socket(&notFoundResponse, client);
    httpserver_set_response_status(&notFoundResponse, 404);

    httpserver_send_response(&notFoundResponse);
  }

  return;

  bad_request: {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 400);

    httpserver_send_response(&response);

    return;
  }

  method_not_allowed: {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[3];
    httpserver_init_response(&response, headerBuffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 405);

    httpserver_send_response(&response);

    return;
  }
}

int websocketCallback(struct csocket_server_client *client, struct frequenc_ws_frame *ws_frame) {
  printf("[main]: WS message: %d\n", csocket_server_client_get_id(client));

  if (ws_frame->opcode == 8) {
    httpserver_disconnect_client(client);

    return 1;
  }

  printf("[main]: Socket: %d\n", csocket_server_client_get_id(client));
  printf("[main]: Opcode: %d\n", ws_frame->opcode);
  printf("[main]: FIN: %d\n", ws_frame->fin);
  printf("[main]: Length: %zu\n", ws_frame->payload_length);
  printf("[main]: Buffer: |%s|\n", ws_frame->buffer);

  return 0;
}

void disconnectCallback(struct csocket_server_client *client, int socket_index) {
  /* TODO: Add resuming mechanism; set client_auth->disconnected to true */
  char socketStr[4];
  snprintf(socketStr, sizeof(socketStr), "%d", csocket_server_client_get_id(client));

  char *sessionId = httpserver_get_socket_data(&server, socket_index);

  if (sessionId == NULL) return;

  cthreads_mutex_lock(&mutex);
  struct tablec_bucket *bucket2 = tablec_get(&clients, sessionId);

  if (bucket2->value == NULL) {
    printf("[main]: Client disconnected not properly. Report it.\n - Reason: Can't find saved sessionId on hashtable.\n - Socket: %d\n - Socket index: %d\n - Thread ID: %lu\n", csocket_server_client_get_id(client), socket_index, cthreads_thread_id(cthreads_thread_self()));

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

  httpserver_stop_server(&server);
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

  httpserver_start_server(&server);

  httpserver_handle_request(&server, callback, websocketCallback, disconnectCallback);
}
