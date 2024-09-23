/*
  (PerformanC's) C(ross-compatible) SSL Library

  License available on: licenses/performanc.license
*/

#ifndef PCLL_H
#define PCLL_H

#include <stdint.h>

#define PCLL_OPENSSL  1
#define PCLL_WOLFSSL  2
#define PCLL_SCHANNEL 3

#if PCLL_SSL_LIBRARY == PCLL_OPENSSL
  #include <openssl/err.h>
  #include <openssl/ssl.h>
  #include <openssl/x509_vfy.h>
  #include <openssl/x509v3.h>
#elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
  #pragma message "Using wolfSSL is experimental and may not work as expected. Security is NOT guaranteed."

  #include <wolfssl/ssl.h>
#elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
  #pragma message "Using SChannel is experimental and may not work as expected. Security is NOT guaranteed. Support for encrypted server is not supported yet."

  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <windows.h>
  #define SECURITY_WIN32
  #include <security.h>
  #include <schannel.h>
  #include <shlwapi.h>
#endif

#include "tcplimits.h"

struct pcll_connection {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL *ssl;
    SSL_CTX *ctx;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    WOLFSSL *ssl;
    WOLFSSL_CTX *ctx;
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    WSADATA wsa_data;
    CredHandle handle;
    SecPkgContext_StreamSizes sizes;
    CtxtHandle context;
    SOCKET socket;
    CtxtHandle *ssl;
    CredHandle *ctx;
    int received;
    int available;
    int used;
    char *decrypted;
    char incoming[TCPLIMITS_PACKET_SIZE];
    char *hostname;
  #else
    /* INFO: This is a dummy structure to avoid compilation errors */
    uint8_t dummy;
  #endif
};

struct pcll_server {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL *ssl;
    SSL_CTX *ctx;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    WOLFSSL *ssl;
    WOLFSSL_CTX *ctx;
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    CtxtHandle *ssl;
    CredHandle *ctx;
  #else
    /* INFO: This is a dummy structure to avoid compilation errors */
    uint8_t dummy;
  #endif
};

#define PCLL_SUCCESS 0
#define PCLL_ERROR  -1

void pcll_init_ssl_library(void);

int pcll_init_ssl(struct pcll_connection *connection);

int pcll_init_only_ssl(struct pcll_connection *connection);

int pcll_init_tls_server(struct pcll_server *server, char *cert, char *key);

#ifdef _WIN32
  int pcll_set_safe_mode(struct pcll_connection* connection, char *hostname, unsigned short port, SOCKET fd);
#else
  int pcll_set_safe_mode(struct pcll_connection *connection, char *hostname, unsigned short port, int fd);
#endif

int pcll_connect(struct pcll_connection *connection);

int pcll_accept(struct pcll_connection *connection);

int pcll_get_error(struct pcll_connection *connection, int error);

int pcll_send(struct pcll_connection *connection, char *data, int length);

int pcll_recv(struct pcll_connection *connection, char *data, int length);

void pcll_free(struct pcll_connection *connection);

void pcll_shutdown(struct pcll_connection *connection);

#endif /* PCLL_H */
