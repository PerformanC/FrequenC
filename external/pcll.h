#ifndef PCLL_H
#define PCLL_H

#define PCLL_OPENSSL 1
#define PCLL_WOLFSSL 2

#if PCLL_SSL_LIBRARY == PCLL_OPENSSL
  #include <openssl/err.h>
  #include <openssl/ssl.h>
  #include <openssl/x509_vfy.h>
  #include <openssl/x509v3.h>
#elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
  #pragma message "Using wolfSSL is experimental and may not work as expected. Security is NOT guaranteed."

  #include <wolfssl/ssl.h>
#endif

struct pcll_connection {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL *ssl;
    SSL_CTX *ctx;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    WOLFSSL *ssl;
    WOLFSSL_CTX *ctx;
  #endif
};

int pcll_init_ssl_library(void);

int pcll_init_ssl(struct pcll_connection *connection);

int pcll_set_fd(struct pcll_connection *connection, int fd);

int pcll_set_safe_mode(struct pcll_connection *connection, char *hostname);

int pcll_connect(struct pcll_connection *connection);

int pcll_get_error(struct pcll_connection *connection);

int pcll_send(struct pcll_connection *connection, char *data, int length);

int pcll_recv(struct pcll_connection *connection, char *data, int length);

void pcll_free(struct pcll_connection *connection);

void pcll_shutdown(struct pcll_connection *connection);

#endif /* PCLL_H */
