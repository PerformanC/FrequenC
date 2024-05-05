/*
  (PerformanC's) C(ross-compatible) SSL Library

  License available on: licenses/performanc.license
*/

#include <stdio.h>
#include <stdlib.h>

#include "pcll.h"

#if PCLL_SSL_LIBRARY == PCLL_OPENSSL
  #include <openssl/err.h>
  #include <openssl/ssl.h>
  #include <openssl/x509_vfy.h>
  #include <openssl/x509v3.h>

  #define SSL_SUCCESS 1
#elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
  #include <wolfssl/ssl.h>
#endif

void pcll_init_ssl_library(void) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    wolfSSL_Init();
  #endif
}

int pcll_init_tls_server(struct pcll_server *server, char *cert, char *key) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    server->ctx = SSL_CTX_new(TLS_server_method());
    if (server->ctx == NULL) {
      SSL_CTX_free(server->ctx);

      return PCLL_ERROR;
    }

    int ret = SSL_CTX_set_min_proto_version(server->ctx, TLS1_3_VERSION);
    if (ret != SSL_SUCCESS) {
      SSL_CTX_free(server->ctx);

      return ret;
    }

    ret = SSL_CTX_use_certificate_file(server->ctx, cert, SSL_FILETYPE_PEM);
    if (ret != SSL_SUCCESS) {
      SSL_CTX_free(server->ctx);

      return ret;
    }

    ret = SSL_CTX_use_PrivateKey_file(server->ctx, key, SSL_FILETYPE_PEM);
    if (ret != SSL_SUCCESS) {
      SSL_CTX_free(server->ctx);

      return ret;
    }

    ret = SSL_CTX_check_private_key(server->ctx);
    if (ret != SSL_SUCCESS) {
      SSL_CTX_free(server->ctx);

      return ret;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    server->ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
    if (server->ctx == NULL) {
      wolfSSL_CTX_free(server->ctx);

      return PCLL_ERROR;
    }

    int ret = wolfSSL_CTX_use_certificate_file(server->ctx, cert, SSL_FILETYPE_PEM);
    if (ret != WOLFSSL_SUCCESS) {
      wolfSSL_CTX_free(server->ctx);

      return ret;
    }

    ret = wolfSSL_CTX_use_PrivateKey_file(server->ctx, key, SSL_FILETYPE_PEM);
    if (ret != WOLFSSL_SUCCESS) {
      wolfSSL_CTX_free(server->ctx);

      return ret;
    }

    ret = wolfSSL_CTX_check_private_key(server->ctx);
    if (ret != WOLFSSL_SUCCESS) {
      wolfSSL_CTX_free(server->ctx);

      return ret;
    }

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_init_ssl(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    connection->ctx = SSL_CTX_new(TLS_client_method());
    if (connection->ctx == NULL) {
      SSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    int ret = SSL_CTX_set_min_proto_version(connection->ctx, TLS1_3_VERSION);
    if (ret != SSL_SUCCESS) {
      SSL_CTX_free(connection->ctx);

      return ret;
    }

    connection->ssl = SSL_new(connection->ctx);
    if (connection->ssl == NULL) {
      SSL_free(connection->ssl);
      SSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    /* TODO: How portable is to use wolfTLSv1_3_client_method? */
    connection->ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (connection->ctx == NULL) {
      wolfSSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    connection->ssl = wolfSSL_new(connection->ctx);
    if (connection->ssl == NULL) {
      wolfSSL_free(connection->ssl);
      wolfSSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_init_only_ssl(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    connection->ssl = SSL_new(connection->ctx);
    if (connection->ssl == NULL) {
      SSL_free(connection->ssl);
      SSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    connection->ssl = wolfSSL_new(connection->ctx);
    if (connection->ssl == NULL) {
      wolfSSL_free(connection->ssl);
      wolfSSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_set_fd(struct pcll_connection *connection, int fd) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    int ret = SSL_set_fd(connection->ssl, fd);
    if (ret != SSL_SUCCESS) {
      SSL_free(connection->ssl);
      SSL_CTX_free(connection->ctx);

      return ret;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    int ret = wolfSSL_set_fd(connection->ssl, fd);
    if (ret != WOLFSSL_SUCCESS) {
      wolfSSL_free(connection->ssl);
      wolfSSL_CTX_free(connection->ctx);

      return ret;
    }

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_set_safe_mode(struct pcll_connection *connection, char *hostname) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL_CTX_set_verify(connection->ctx, SSL_VERIFY_PEER, NULL);

    /* TODO: Get SSL root and CA trust store on PCLL */
    int ret = SSL_CTX_set_default_verify_paths(connection->ctx);
    if (ret != SSL_SUCCESS) return ret;

    ret = SSL_set1_host(connection->ssl, hostname);
    if (ret != SSL_SUCCESS) return ret;

    ret = SSL_set_tlsext_host_name(connection->ssl, hostname);
    if (ret != SSL_SUCCESS) return ret;

    ret = SSL_get_verify_result(connection->ssl);
    if (ret != X509_V_OK) return ret;

    return 0;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    (void)hostname; /* No SNI */

    wolfSSL_CTX_set_verify(connection->ctx, WOLFSSL_VERIFY_PEER, NULL);

    int ret = wolfSSL_CTX_load_system_CA_certs(connection->ctx);
    if (ret != WOLFSSL_SUCCESS) return ret;

    /* TODO: WolfSSL SNI support */

    if (connection->ssl == NULL) return PCLL_ERROR;

    return 0;
  #endif

  return PCLL_ERROR;
}

int pcll_connect(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    int ret = SSL_connect(connection->ssl);
    if (ret != SSL_SUCCESS) return ret;

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    int ret = wolfSSL_connect(connection->ssl);
    if (ret != WOLFSSL_SUCCESS) return ret;

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_accept(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    int ret = SSL_accept(connection->ssl);
    if (ret != SSL_SUCCESS) {
      SSL_free(connection->ssl);
      SSL_CTX_free(connection->ctx);

      return ret;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    int ret = wolfSSL_accept(connection->ssl);
    if (ret != WOLFSSL_SUCCESS) {
      wolfSSL_free(connection->ssl);
      wolfSSL_CTX_free(connection->ctx);

      return ret;
    }

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_get_error(struct pcll_connection *connection, int error) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    return SSL_get_error(connection->ssl, error);
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    return wolfSSL_get_error(connection->ssl, error);
  #endif

  return PCLL_ERROR;
}

int pcll_send(struct pcll_connection *connection, char *data, int length) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    int ret = SSL_write(connection->ssl, data, length);
    if (ret != length) {
      /* TODO: Should we clean up here or let the dev decide? */
      SSL_free(connection->ssl);
      SSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    int ret = wolfSSL_write(connection->ssl, data, length);
    if (ret != length) {
      /* TODO: Should we clean up here or let the dev decide? */
      wolfSSL_free(connection->ssl);
      wolfSSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return PCLL_SUCCESS;
  #endif

  return PCLL_ERROR;
}

int pcll_recv(struct pcll_connection *connection, char *data, int length) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    int recv_length = SSL_read(connection->ssl, data, length);
    if (recv_length == -1) {
      /* TODO: Should we clean up here or let the dev decide? */
      SSL_free(connection->ssl);
      SSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return recv_length;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    int recv_length = wolfSSL_read(connection->ssl, data, length);
    if (recv_length == -1) {
      /* TODO: Should we clean up here or let the dev decide? */
      wolfSSL_free(connection->ssl);
      wolfSSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    return recv_length;
  #endif

  return PCLL_ERROR;
}

void pcll_free(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    if (connection->ssl != NULL) SSL_free(connection->ssl);
    if (connection->ctx != NULL) SSL_CTX_free(connection->ctx);
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    wolfSSL_free(connection->ssl);
    wolfSSL_CTX_free(connection->ctx);
    wolfSSL_Cleanup();
  #endif
}

void pcll_shutdown(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL_shutdown(connection->ssl);
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    wolfSSL_shutdown(connection->ssl);
  #endif
}
