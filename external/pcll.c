/*
  (PerformanC's) C(ross-compatible) SSL Library

  License available on: licenses/performanc.license
*/

#include <stdio.h>
#include <stdlib.h>

#include "tcplimits.h"

#include "pcll.h"

#if PCLL_SSL_LIBRARY == PCLL_OPENSSL
  #include <openssl/err.h>
  #include <openssl/ssl.h>
  #include <openssl/x509_vfy.h>
  #include <openssl/x509v3.h>

  #define SSL_SUCCESS 1
#elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
  #include <wolfssl/ssl.h>
#elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <windows.h>
  #define SECURITY_WIN32
  #include <security.h>
  #include <schannel.h>
  #include <shlwapi.h>
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    /* TODO: Implement */

    return PCLL_SUCCESS;
  #else
    return PCLL_ERROR;
  #endif
}

int pcll_init_ssl(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    connection->ctx = SSL_CTX_new(TLS_client_method());
    if (connection->ctx == NULL) {
      SSL_CTX_free(connection->ctx);

      return PCLL_ERROR;
    }

    /*
      This is set to TLS v1.2 as that's the maximum Discord accepts. ONLY put as v1.2 if
        the targeted service doesn't accept it.
    */
    int ret = SSL_CTX_set_min_proto_version(connection->ctx, TLS1_2_VERSION);
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    if (WSAStartup(MAKEWORD(2, 2), &connection->wsa_data) != 0) return PCLL_ERROR;

    return PCLL_SUCCESS;
  #endif
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    if (WSAStartup(MAKEWORD(2, 2), &connection->wsa_data) != 0) return PCLL_ERROR;

    return PCLL_SUCCESS;
  #else
    return PCLL_ERROR;
  #endif
}

#ifdef _WIN32
  int pcll_set_safe_mode(struct pcll_connection* connection, char* hostname, unsigned short port, SOCKET fd) {
#else
  int pcll_set_safe_mode(struct pcll_connection* connection, char* hostname, unsigned short port, int fd) {
#endif
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    (void) port;

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

    ret = SSL_set_fd(connection->ssl, fd);
    if (ret != SSL_SUCCESS) {
        SSL_free(connection->ssl);
        SSL_CTX_free(connection->ctx);

        return ret;
    }

    return PCLL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    (void) port;
    (void) hostname; /* No SNI */

    wolfSSL_CTX_set_verify(connection->ctx, WOLFSSL_VERIFY_PEER, NULL);

    int ret = wolfSSL_CTX_load_system_CA_certs(connection->ctx);
    if (ret != WOLFSSL_SUCCESS) return ret;

    /* TODO: WolfSSL SNI support */

    if (connection->ssl == NULL) return PCLL_ERROR;

    ret = wolfSSL_set_fd(connection->ssl, fd);
    if (ret != WOLFSSL_SUCCESS) {
        wolfSSL_free(connection->ssl);
        wolfSSL_CTX_free(connection->ctx);

        return ret;
    }

    return PCCL_SUCCESS;
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    char port_str[64];
    wnsprintfA(port_str, sizeof(port_str), "%u", port);

    if (!WSAConnectByNameA(fd, hostname, port_str, NULL, NULL, NULL, NULL, NULL, NULL)) return PCLL_ERROR;

    SCHANNEL_CRED cred = {
      .dwVersion = SCHANNEL_CRED_VERSION,
      .dwFlags = SCH_USE_STRONG_CRYPTO
                  | SCH_CRED_AUTO_CRED_VALIDATION
                  | SCH_CRED_NO_DEFAULT_CREDS,
      .grbitEnabledProtocols = SP_PROT_TLS1_2,
    };

    if (AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &connection->handle, NULL) != SEC_E_OK) return PCLL_ERROR;

    connection->socket = fd;
    connection->hostname = hostname;
    connection->received = 0;
    connection->available = 0;
    connection->used = 0;
    connection->decrypted = NULL;

    return PCLL_SUCCESS;
  #else
    return PCLL_ERROR;
  #endif
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    CtxtHandle *context = NULL;

    while (1) {
      SecBuffer inbuffers[2] = { 0 };
      inbuffers[0].BufferType = SECBUFFER_TOKEN;
      inbuffers[0].pvBuffer = connection->incoming;
      inbuffers[0].cbBuffer = connection->received;
      inbuffers[1].BufferType = SECBUFFER_EMPTY;

      SecBuffer outbuffers[1] = { 0 };
      outbuffers[0].BufferType = SECBUFFER_TOKEN;

      SecBufferDesc indesc = { SECBUFFER_VERSION, ARRAYSIZE(inbuffers), inbuffers };
      SecBufferDesc outdesc = { SECBUFFER_VERSION, ARRAYSIZE(outbuffers), outbuffers };

      DWORD flags = ISC_REQ_USE_SUPPLIED_CREDS | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
      SECURITY_STATUS sec = InitializeSecurityContextA(
        &connection->handle,
        context,
        context ? NULL : (SEC_CHAR *)connection->hostname,
        flags,
        0,
        0,
        context ? &indesc : NULL,
        0,
        context ? NULL : &connection->context,
        &outdesc,
        &flags,
        NULL);

      context = &connection->context;

      if (inbuffers[1].BufferType == SECBUFFER_EXTRA) {
        /* TODO: use ANSI C function */
        MoveMemory(connection->incoming, connection->incoming + (connection->received - inbuffers[1].cbBuffer), inbuffers[1].cbBuffer);

        connection->received = inbuffers[1].cbBuffer;
      }

      else {
        connection->received = 0;
      }

      if (sec == SEC_E_OK) break;

      else if (sec == SEC_I_INCOMPLETE_CREDENTIALS) break;

      else if (sec == SEC_I_CONTINUE_NEEDED) {
        char *buffer = outbuffers[0].pvBuffer;
        int size = outbuffers[0].cbBuffer;

        while (size != 0) {
          int d = send(connection->socket, buffer, size, 0);
          if (d <= 0) break;

          size -= d;
          buffer += d;
        }

        FreeContextBuffer(outbuffers[0].pvBuffer);

        if (size != 0) goto fail;
      }

      else if (sec != SEC_E_INCOMPLETE_MESSAGE) goto fail;

      if (connection->received == sizeof(connection->incoming)) goto fail;

      int r = recv(connection->socket, connection->incoming + connection->received, sizeof(connection->incoming) - connection->received, 0);
      if (r == 0) return PCLL_SUCCESS;

      else if (r < 0) goto fail;

      connection->received += r;
    }

    QueryContextAttributes(context, SECPKG_ATTR_STREAM_SIZES, &connection->sizes);

    return PCLL_SUCCESS;

    fail:
      DeleteSecurityContext(context);
      FreeCredentialsHandle(&connection->handle); /* TODO: Maybe move to pcll_free */

      return PCLL_ERROR;
  #else
    return PCLL_ERROR;
  #endif
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    /* TODO: Implement */

    return PCLL_SUCCESS;
  #else
    return PCLL_ERROR;
  #endif
}

int pcll_get_error(struct pcll_connection *connection, int error) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    return SSL_get_error(connection->ssl, error);
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    return wolfSSL_get_error(connection->ssl, error);
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    (void) connection;

    return error;
  #else
    return PCLL_ERROR;
  #endif
}

int pcll_send(struct pcll_connection* connection, char* data, int length) {
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    while (length != 0) {
      int use = min(length, connection->sizes.cbMaximumMessage);

      /* TODO: Reduce memory allocation */
      char wbuffer[TCPLIMITS_PACKET_SIZE];

      SecBuffer buffers[3];
      buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
      buffers[0].pvBuffer = wbuffer;
      buffers[0].cbBuffer = connection->sizes.cbHeader;
      buffers[1].BufferType = SECBUFFER_DATA;
      buffers[1].pvBuffer = wbuffer + connection->sizes.cbHeader;
      buffers[1].cbBuffer = use;
      buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
      buffers[2].pvBuffer = wbuffer + connection->sizes.cbHeader + use;
      buffers[2].cbBuffer = connection->sizes.cbTrailer;

      memcpy(buffers[1].pvBuffer, data, use);

      SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };
      SECURITY_STATUS sec = EncryptMessage(&connection->context, 0, &desc, 0);
      if (sec != SEC_E_OK) return PCLL_ERROR;

      int total = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
      int sent = 0;

      while (sent != total) {
        int ret = send(connection->socket, wbuffer + sent, total - sent, 0);
        if (ret <= 0) return PCLL_ERROR;

        sent += ret;
      }

      data = data + use;
      length -= use;
    }

    return PCLL_SUCCESS;
  #else
    return PCLL_ERROR;
  #endif
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
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    int result = PCLL_SUCCESS;

    while (length != 0) {
      if (connection->decrypted != NULL) {
        int use = min(length, connection->available);
        memcpy((void *)data, connection->decrypted, use);

        data = data + use;
        length -= use;
        result += use;

        if (use == connection->available) {
          memmove(connection->incoming, connection->incoming + connection->used, connection->received - connection->used);
          connection->received -= connection->used;
          connection->used = 0;
          connection->available = 0;
          connection->decrypted = NULL;
        } else {
          connection->available -= use;
          connection->decrypted += use;
        }
      } else {
        if (connection->received != 0) {
          SecBuffer buffers[4];

          buffers[0].BufferType = SECBUFFER_DATA;
          buffers[0].pvBuffer = connection->incoming;
          buffers[0].cbBuffer = connection->received;
          buffers[1].BufferType = SECBUFFER_EMPTY;
          buffers[2].BufferType = SECBUFFER_EMPTY;
          buffers[3].BufferType = SECBUFFER_EMPTY;

          SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };

          SECURITY_STATUS sec = DecryptMessage(&connection->context, &desc, 0, NULL);
          if (sec == SEC_E_OK) {
            connection->decrypted = buffers[1].pvBuffer;
            connection->available = buffers[1].cbBuffer;
            connection->used = connection->received - (buffers[3].BufferType == SECBUFFER_EXTRA ? buffers[3].cbBuffer : 0);

            continue;
          }

          else if (sec == SEC_I_CONTEXT_EXPIRED) {
            printf("[PCLL]: Failed to decrypt message: context expired\n");

            connection->received = 0;

            return result;
          }

          else if (sec == SEC_I_RENEGOTIATE) {
            printf("[PCLL]: Failed to decrypt message: renegotiation required\n");

            return PCLL_ERROR;
          }

          else if (sec != SEC_E_INCOMPLETE_MESSAGE) {
            printf("[PCLL]: Failed to decrypt message: invalid message\n");

            return PCLL_ERROR;
          }
        }

        if (result != PCLL_SUCCESS) break;

        if (connection->received == sizeof(connection->incoming)) {
          printf("[PCLL]: Failed to decrypt message: buffer full\n");

          return PCLL_ERROR;
        }

        int ret = recv(connection->socket, connection->incoming + connection->received, sizeof(connection->incoming) - connection->received, 0);

        if (ret == 0) return PCLL_SUCCESS;
        else if (ret < 0) return PCLL_ERROR;

        connection->received += ret;
      }
    }

    return result;
  #else
    return PCLL_ERROR;
  #endif
}

void pcll_free(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    if (connection->ssl != NULL) SSL_free(connection->ssl);
    if (connection->ctx != NULL) SSL_CTX_free(connection->ctx);
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    wolfSSL_free(connection->ssl);
    wolfSSL_CTX_free(connection->ctx);
    wolfSSL_Cleanup();
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    WSACleanup();
  #endif
}

void pcll_shutdown(struct pcll_connection *connection) {
  #if PCLL_SSL_LIBRARY == PCLL_OPENSSL
    SSL_shutdown(connection->ssl);
  #elif PCLL_SSL_LIBRARY == PCLL_WOLFSSL
    wolfSSL_shutdown(connection->ssl);
  #elif PCLL_SSL_LIBRARY == PCLL_SCHANNEL
    DWORD type = SCHANNEL_SHUTDOWN;

    SecBuffer inbuffers[1];
    inbuffers[0].BufferType = SECBUFFER_TOKEN;
    inbuffers[0].pvBuffer = &type;
    inbuffers[0].cbBuffer = sizeof(type);

    SecBufferDesc indesc = { SECBUFFER_VERSION, ARRAYSIZE(inbuffers), inbuffers };
    ApplyControlToken(&connection->context, &indesc);

    SecBuffer outbuffers[1];
    outbuffers[0].BufferType = SECBUFFER_TOKEN;

    SecBufferDesc outdesc = { SECBUFFER_VERSION, ARRAYSIZE(outbuffers), outbuffers };
    DWORD flags = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
    if (InitializeSecurityContextA(&connection->handle, &connection->context, NULL, flags, 0, 0, &outdesc, 0, NULL, &outdesc, &flags, NULL) == SEC_E_OK) {
      char *buffer = outbuffers[0].pvBuffer;
      int size = outbuffers[0].cbBuffer;

      int ret = send(connection->socket, buffer, size, 0);
      if (ret != size) {
        /* It will close anyway */
      }

      FreeContextBuffer(outbuffers[0].pvBuffer);
    }

    DeleteSecurityContext(&connection->context);
    FreeCredentialsHandle(&connection->handle);
  #endif
}
