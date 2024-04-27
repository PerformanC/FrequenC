#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "httpserver.h"
#include "websocket.h"
#define JSMN_HEADER
#include "jsmn-find.h"
#include "tablec.h"
#include "queryparser.h"
#include "urldecode.h"
#include "cthreads.h"
#include "csocket-server.h"
#include "pdvoice.h"
#include "types.h"

/* Sources */
#include "youtube.h"
/* Sources end */
#include "track.h"
#include "utils.h"

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

struct client_guild_vc_info {
  char *session_id;
  char *token;
  char *endpoint;
};

struct client_guild_information {
  unsigned long guild_id;
  struct client_guild_vc_info *vc_info;
};

struct client_authorization {
  char *user_id;
  struct csocket_server_client *client;
  bool disconnected;
  struct tablec_ht guilds;
};

struct tablec_ht clients;
struct httpserver server;
struct cthreads_mutex mutex;

void callback(struct csocket_server_client *client, int socket_index, struct httpparser_request *request) {
  struct httpparser_header *authorization = httpparser_get_header(request, "Authorization");

  if (authorization == NULL || strcmp(authorization->value, AUTHORIZATION) != 0) {
    printf("CALBACKKKKKKKKKKKKKKKKKKKK ERROR UNAUTHORIZED\n");
    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

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
    if (header == NULL || strcmp(header->value, "websocket") != 0) {
      printf("[main]: No Upgrade header found.\n");

      goto bad_request;
    }

    struct httpparser_header *sec_websocket_key = httpparser_get_header(request, "Sec-WebSocket-Key");
    if (sec_websocket_key == NULL) {
      printf("[main]: No Sec-WebSocket-Key header found.\n");

      goto bad_request;
    }

    struct httpparser_header *user_id = httpparser_get_header(request, "User-Id");
    if (user_id == NULL) {
      printf("[main]: No User-Id header found.\n");

      goto bad_request;
    }

    struct httpparser_header *client_info = httpparser_get_header(request, "Client-Info");
    if (client_info == NULL) {
      printf("[main]: No Client-Info header found.\n");

      goto bad_request;
    }

    struct frequenc_client_info parsed_client_info;
    if (frequenc_parse_client_info(client_info->value, &parsed_client_info) == -1) {
      printf("[main]: Failed to parse Client-Info header.\n");

      goto bad_request;
    }

    printf("[main]: WebSocket connection accepted.\n - Name: %s\n - Version: %s\n - Bot name: %s\n", parsed_client_info.name, parsed_client_info.version, parsed_client_info.bot_name);

    frequenc_free_client_info(&parsed_client_info);

    struct httpparser_header *session_id = httpparser_get_header(request, "Session-Id");

    struct httpserver_response response;
    struct httpserver_header headers_buffer[5];
    httpserver_init_response(&response, headers_buffer, 5);

    char accept_key[32 + 1];
    frequenc_gen_accept_key(sec_websocket_key->value, accept_key);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 101);
    httpserver_set_response_header(&response, "Upgrade", "websocket");
    httpserver_set_response_header(&response, "Connection", "Upgrade");
    httpserver_set_response_header(&response, "Sec-WebSocket-Accept", accept_key);
    httpserver_set_response_header(&response, "Sec-WebSocket-Version", "13");

    httpserver_send_response(&response);

    struct tablec_bucket *selected_client;

    cthreads_mutex_lock(&mutex);
    if (session_id != NULL && (selected_client = tablec_get(&clients, session_id->value)) != NULL && ((struct client_authorization *)selected_client->value)->disconnected) {
      cthreads_mutex_unlock(&mutex);

      struct client_authorization *client_auth = selected_client->value;

      client_auth->user_id = user_id->value;
      client_auth->client = client;

      char payload[1024];
      int payload_length = snprintf(payload, sizeof(payload),
        "{"
          "\"op\":\"ready\","
          "\"resumed\":true,"
          "\"session_id\":\"%s\""
        "}", session_id->value
      );

      struct frequenc_ws_message ws_response;
      ws_response.opcode = 1;
      ws_response.fin = 1;
      ws_response.payload_length = payload_length;
      ws_response.buffer = payload;
      ws_response.client = client;

      frequenc_send_ws_response(&ws_response);

      httpserver_set_socket_data(&server, socket_index, session_id->value);

      httpserver_upgrade_socket(&server, socket_index);

      return;
    }

    cthreads_mutex_unlock(&mutex);

    struct client_authorization *client_auth = frequenc_safe_malloc(sizeof(struct client_authorization));

    char *gen_session_id = frequenc_safe_malloc((16 + 1) * sizeof(char));

    generateSession:
      frequenc_generate_session_id(gen_session_id);

      if (tablec_get(&clients, gen_session_id) != NULL) {
        printf("[main]: Session ID collision detected. Generating another one.\n");

        goto generateSession;
      }
    
    client_auth->user_id = user_id->value;
    client_auth->client = client;
    client_auth->disconnected = false;
    
    struct tablec_bucket *guilds = frequenc_safe_malloc(sizeof(struct tablec_bucket));
    tablec_init(&client_auth->guilds, guilds, 1);

    char payload[1024];
    int payload_length = snprintf(payload, sizeof(payload),
      "{"
        "\"op\":\"ready\","
        "\"resumed\":false,"
        "\"session_id\":\"%s\""
      "}", gen_session_id
    );

    struct frequenc_ws_message ws_response;
    ws_response.opcode = 1;
    ws_response.fin = 1;
    ws_response.payload_length = payload_length;
    ws_response.buffer = payload;
    ws_response.client = client;

    frequenc_send_ws_response(&ws_response);

    cthreads_mutex_lock(&mutex);
    if (tablec_set(&clients, gen_session_id, client_auth) == -1) {
      cthreads_mutex_unlock(&mutex);
      printf("[main]: Hashtable is full, resizing.\n");

      struct tablec_ht old_clients = clients;
      size_t new_capacity = clients.capacity * 2;
      struct tablec_bucket *new_buckets = frequenc_safe_malloc(new_capacity * sizeof(struct tablec_bucket));

      cthreads_mutex_lock(&mutex);
      if (tablec_resize(&clients, new_buckets, new_capacity) == -1) {
        printf("[main]: Hashtable resizing failed. Exiting.\n");

        exit(1);
      }
      cthreads_mutex_unlock(&mutex);

      free(old_clients.buckets);

      cthreads_mutex_lock(&mutex);
      tablec_set(&clients, gen_session_id, client_auth);
    }
    cthreads_mutex_unlock(&mutex);

    httpserver_set_socket_data(&server, socket_index, gen_session_id);

    httpserver_upgrade_socket(&server, socket_index);
  }

  else if (strcmp(request->path, "/version") == 0) {
    if (strcmp(request->method, "GET") != 0) goto method_not_allowed;

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

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
    char payload_length[4];
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
      payload_length,
      sizeof(payload_length)
    );

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payload_length);
    httpserver_set_response_body(&response, payload);

    httpserver_send_response(&response);
  }

  else if (strcmp(request->path, "/v1/decodetracks") == 0) {
    if (strcmp(request->method, "POST") != 0) goto method_not_allowed;

    if (!request->body) {
      printf("[main]: No body found.\n");

      goto bad_request;
    }

    struct httpparser_header *content_length = httpparser_get_header(request, "Content-Length");

    jsmn_parser parser;
    jsmntok_t *tokens = NULL;
    unsigned num_tokens = 0;

    jsmn_init(&parser);
    int r = jsmn_parse_auto(&parser, request->body, atoi(content_length->value), &tokens, &num_tokens);
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

    char payload_length[8 + 1];
    frequenc_stringify_int(arrayResponseLength, payload_length, sizeof(payload_length));

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payload_length);
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
    char payload_length[3 + 1];
    frequenc_stringify_int(frequenc_track_info_to_json(&decoded_track, decoded_query, payload, sizeof(payload)), payload_length, sizeof(payload_length));

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payload_length);
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

    struct httpparser_header *content_length = httpparser_get_header(request, "Content-Length");

    jsmn_parser parser;
    jsmntok_t *tokens = NULL;
    unsigned num_tokens = 0;

    jsmn_init(&parser);
    int r = jsmn_parse_auto(&parser, request->body, atoi(content_length->value), &tokens, &num_tokens);
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
      free(pairs);
      free(tokens);
      
      printf("[main]: Failed to load JSON: %d\n", r);

      goto bad_request;
    }

    if (pairs->size == 0) {
      free(pairs);
      free(tokens);

      printf("[main]: No tracks found in the body.\n");

      goto bad_request;
    }

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
        free(pairs);
        free(tokens);

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

    char payload_length[8 + 1];
    frequenc_stringify_int(arrayResponseLength, payload_length, sizeof(payload_length));

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payload_length);
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

    struct httpparser_header *content_length = httpparser_get_header(request, "Content-Length");

    jsmn_init(&parser);
    int r = jsmn_parse(&parser, request->body, atoi(content_length->value), tokens, 32);
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
    char payload_length[4];
    frequenc_stringify_int(frequenc_encode_track(&decoded_track, &encoded_track), payload_length, sizeof(payload_length));

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "text/plain");
    httpserver_set_response_header(&response, "Content-Length", payload_length);
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

    printf("[main]: Load tracks request: %s\n", identifierDecoded);

    struct tstr_string result = { 0 };

    if (strncmp(identifierDecoded, "ytsearch:", sizeof("ytsearch:") - 1) == 0)
      result = frequenc_youtube_search(identifierDecoded + (sizeof("ytsearch:") - 1), 0);

    /* add ytm support*/
    // if (strncmp(identifierDecoded, "ytmsearch:", sizeof("ytmsearch:") - 1) == 0)
    //   result = frequenc_youtube_search(identifierDecoded + (sizeof("ytmsearch:") - 1), 1);

    char payload_length[8 + 1];
    frequenc_stringify_int(result.length, payload_length, sizeof(payload_length));

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);
    httpserver_set_response_header(&response, "Content-Type", "application/json");
    httpserver_set_response_header(&response, "Content-Length", payload_length);
    httpserver_set_response_body(&response, result.string);

    httpserver_send_response(&response);

    if (result.allocated)
      free(result.string);
  }

  else if (strncmp(request->path, "/v1/sessions/", sizeof("/v1/sessions") - 1) == 0) {
    struct tstr_string_token session_id;
    tstr_find_between(&session_id, request->path, NULL, (size_t)sizeof("/v1/sessions/") - 1, "/", 0);

    if (session_id.start == 0 || session_id.end == 0) {
      printf("[main]: No session ID found.\n");

      goto bad_request;
    }

    char session_id_str[16 + 1];
    frequenc_fast_copy(request->path + session_id.start, session_id_str, session_id.end - session_id.start);

    struct tablec_bucket *session_bucket = tablec_get(&clients, session_id_str);

    if (session_bucket == NULL) {
      printf("[main]: Session ID not found.\n");

      goto bad_request;
    }

    struct client_authorization *client_auth = session_bucket->value;

    if (client_auth->disconnected) {
      printf("[main]: Cannot access a disconnected session.\n");

      goto bad_request;
    }

    struct tstr_string_token guild_id;
    tstr_find_between(&guild_id, request->path, "/players/", session_id.end, "\0", 0);

    if (guild_id.start == 0 || guild_id.end == 0) {
      printf("[main]: No guild ID found.\n");

      goto bad_request;
    }

    char guild_id_str[19 + 1];
    frequenc_fast_copy(request->path + guild_id.start, guild_id_str, guild_id.end - guild_id.start);

    struct tablec_bucket *guild_bucket = tablec_get(&client_auth->guilds, guild_id_str);
    struct client_guild_information *guild_info = NULL;

    if (guild_bucket == NULL) {
      if (strcmp(request->method, "GET") == 0) {
        printf("[main]: Guild ID not found.\n");

        struct httpserver_response response;
        struct httpserver_header headers_buffer[3];
        httpserver_init_response(&response, headers_buffer, 3);

        httpserver_set_response_socket(&response, client);
        httpserver_set_response_status(&response, 404);

        httpserver_send_response(&response);

        return;
      }

      if (tablec_full(&client_auth->guilds) == -1) {
        struct tablec_bucket *old_guild_buckets = client_auth->guilds.buckets;
        struct tablec_bucket *new_guild_buckets = frequenc_safe_malloc(sizeof(struct tablec_bucket) * client_auth->guilds.capacity * 2);

        cthreads_mutex_lock(&mutex);
        if (tablec_resize(&client_auth->guilds, new_guild_buckets, client_auth->guilds.capacity * 2) == -1) {
          cthreads_mutex_unlock(&mutex);

          printf("[main]: Guilds hashtable resizing failed. Exiting.\n");

          exit(1);
        }
        cthreads_mutex_unlock(&mutex);

        free(old_guild_buckets);
      }

      guild_info = frequenc_safe_malloc(sizeof(struct client_guild_information));
      guild_info->guild_id = strtol(guild_id_str, NULL, 10);

      cthreads_mutex_lock(&mutex);
      tablec_set(&client_auth->guilds, guild_id_str, guild_info);
      cthreads_mutex_unlock(&mutex);
    } else {
      guild_info = guild_bucket->value;
    }

    jsmn_parser parser;
    jsmntok_t *tokens = NULL;
    unsigned num_tokens = 0;

    jsmn_init(&parser);
    int r = jsmn_parse_auto(&parser, request->body, request->body_length, &tokens, &num_tokens);
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

    jsmnf_pair *voice = jsmnf_find(pairs, request->body, "voice", sizeof("voice") - 1);

    if (voice == NULL) {
      free(tokens);
      free(pairs);

      printf("[main]: No voice object found.\n");

      goto bad_request;
    }

    jsmnf_pair *token = jsmnf_find(voice, request->body, "token", sizeof("token") - 1);

    if (token == NULL) {
      free(tokens);
      free(pairs);

      printf("[main]: No token field found.\n");

      goto bad_request;
    }

    jsmnf_pair *endpoint = jsmnf_find(voice, request->body, "endpoint", sizeof("endpoint") - 1);

    if (endpoint == NULL) {
      free(tokens);
      free(pairs);

      printf("[main]: No endpoint field found.\n");

      goto bad_request;
    }

    // "session_id" field
    jsmnf_pair *discord_session_id = jsmnf_find(voice, request->body, "session_id", sizeof("session_id") - 1);

    if (discord_session_id == NULL) {
      free(tokens);
      free(pairs);

      printf("[main]: No session_id field found.\n");

      goto bad_request;
    }

    char *voice_token = frequenc_safe_malloc(token->v.len + 1);
    char *voice_endpoint = frequenc_safe_malloc(endpoint->v.len + 1);
    char *voice_session_id = frequenc_safe_malloc(discord_session_id->v.len + 1);

    frequenc_fast_copy(request->body + token->v.pos, voice_token, token->v.len);
    frequenc_fast_copy(request->body + endpoint->v.pos, voice_endpoint, endpoint->v.len);
    frequenc_fast_copy(request->body + discord_session_id->v.pos, voice_session_id, discord_session_id->v.len);

    guild_info->vc_info = frequenc_safe_malloc(sizeof(struct client_guild_vc_info));
    guild_info->vc_info->token = voice_token;
    guild_info->vc_info->endpoint = voice_endpoint;
    guild_info->vc_info->session_id = voice_session_id;

    free(tokens);
    free(pairs);

    printf("[main]: Voice connection received.\n - Guild ID: %s\n - Token: %s\n - Endpoint: %s\n - Session ID: %s\n", guild_id_str, guild_info->vc_info->token, guild_info->vc_info->endpoint, guild_info->vc_info->session_id);

    size_t guild_id_str_length = strlen(guild_id_str);
    char *guild_id_str_alloced = frequenc_safe_malloc(guild_id_str_length + 1);
    frequenc_fast_copy(guild_id_str, guild_id_str_alloced, guild_id_str_length);

    struct pdvoice *client_vc = frequenc_safe_malloc(sizeof(struct pdvoice));
    client_vc->bot_id = client_auth->user_id;
    client_vc->guild_id = guild_id_str_alloced;

    pdvoice_init(client_vc);

    pdvoice_update_state(client_vc, guild_info->vc_info->session_id);
    pdvoice_update_server(client_vc, guild_info->vc_info->endpoint, guild_info->vc_info->token);

    pdvoice_connect(client_vc);

    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 200);

    httpserver_send_response(&response);

    return;
  }

  else {
    struct httpserver_response notFoundResponse;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&notFoundResponse, headers_buffer, 2);

    httpserver_set_response_socket(&notFoundResponse, client);
    httpserver_set_response_status(&notFoundResponse, 404);

    httpserver_send_response(&notFoundResponse);
  }

  return;

  bad_request: {
    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 400);

    httpserver_send_response(&response);

    return;
  }

  method_not_allowed: {
    struct httpserver_response response;
    struct httpserver_header headers_buffer[3];
    httpserver_init_response(&response, headers_buffer, 3);

    httpserver_set_response_socket(&response, client);
    httpserver_set_response_status(&response, 405);

    httpserver_send_response(&response);

    return;
  }
}

int ws_callback(struct csocket_server_client *client, struct frequenc_ws_frame *ws_frame) {
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

void disconnect_callback(struct csocket_server_client *client, int socket_index) {
  /* TODO: Add resuming mechanism; set client_auth->disconnected to true */
  char socketStr[4];
  snprintf(socketStr, sizeof(socketStr), "%d", csocket_server_client_get_id(client));

  char *session_id = httpserver_get_socket_data(&server, socket_index);

  if (session_id == NULL) return;

  cthreads_mutex_lock(&mutex);
  struct tablec_bucket *bucket2 = tablec_get(&clients, session_id);

  if (bucket2->value == NULL) {
    printf("[main]: Client disconnected not properly. Report it.\n - Reason: Can't find saved session_id on hashtable.\n - Socket: %d\n - Socket index: %d\n - Thread ID: %lu\n", csocket_server_client_get_id(client), socket_index, cthreads_thread_id(cthreads_thread_self()));

    exit(1);

    return;
  }

  tablec_del(&clients, bucket2->key);
  cthreads_mutex_unlock(&mutex);

  free(bucket2->value);
  free(session_id);

  return;
}

void handle_sig_int(int sig) {
  (void) sig;

  printf("\n[main]: Stopping server. Checking the hashtable...\n");

  bool detected = false;
  for (size_t i = 0; i < clients.capacity; i++) {
    struct tablec_bucket bucket = clients.buckets[i];

    if (bucket.key == NULL) continue;

    printf("[main]: Found data in the hashtable.\n - Key: %s\n - Value: %p\n", bucket.key, bucket.value);

    detected = true;
  }

  if (!detected) printf("[main]: No data found in the hashtable.\n");
  else printf("[main]: Data found in the hashtable. If there weren't clients connected, please, contribute to the project and report that bug.\n");

  httpserver_stop_server(&server);
  free(clients.buckets);

  printf("[main]: Checking done. Server stopped. Goodbye.\n");
}

int main(void) {
  #if !__linux__ && !_WIN32 && ALLOW_UNSECURE_RANDOM
    printf("[main]: Warning! Using unsecure random seed. If your system supports /dev/urandom, disable ALLOW_UNSECURE_RANDOM and compile again.\n");
  #endif

  signal(SIGINT, handle_sig_int);
  struct tablec_bucket *buckets = frequenc_safe_malloc(sizeof(struct tablec_bucket) * 2);
  tablec_init(&clients, buckets, 2);

  cthreads_mutex_init(&mutex, NULL);

  httpserver_start_server(&server);

  httpserver_handle_request(&server, callback, ws_callback, disconnect_callback);
}
