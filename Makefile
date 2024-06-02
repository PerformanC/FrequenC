CC ?= clang

# FrequenC Runtime Options
PORT = 8888# Port that FrequenC will listen on.
AUTHORIZATION = youshallnotpass# Authorization header for the server. Secure it with a strong password.
AUDIO_QUALITY = 1# Audio quality. 1 = Best, 2 = Good, 3 = Normal, 4 = Bad, 5 = Worst.

# Experiments
TCPLIMITS_EXPERIMENTAL_SAVE_MEMORY = 1 # Save memory by using a lower layers limits instead of the TCP limit itself. 

# Security
# CSOCKET_SECURE = 1 # Secure the connection with SSL/TLS.
# CSOCKET_KEY = "key.pem" # SSL/TLS key.
# CSOCKET_CERT = "cert.pem" # SSL/TLS certificate.
ALLOW_UNSECURE_RANDOM = 0# Only use this for unsupported platforms.

# SSL/TLS
OPENSSL=1
WOLFSSL=2

PCLL_SSL_LIBRARY = $(OPENSSL)# The SSL/TLS library that is used by FrequenC.
SSL_FLAGS = $(if $(filter $(PCLL_SSL_LIBRARY),$(OPENSSL)),-lssl -lcrypto,) $(if $(filter $(PCLL_SSL_LIBRARY),$(WOLFSSL)),-lwolfssl,)# SSL/TLS flags.

# Development
HARDCODED_SESSION_ID = 0# Hardcoded session ID for debugging purposes.

# Github Actions
GITHUB_COMMIT_SHA ?= unknown# The commit hash that the build was made on.
GITHUB_BRANCH ?= unknown# The branch that the commit was made on.

SRC_DIR = lib external sources
OBJ_DIR = obj

CFLAGS ?= -std=c99 -Ofast -march=native -fno-signed-zeros -fno-trapping-math -funroll-loops

ifeq ($(CC),gcc)
	CFLAGS += -fanalyzer -Wno-analyzer-fd-leak -flto -Wpedantic -Wall -Wextra -Werror -Wformat -Wuninitialized -Wshadow -Wno-stringop-overread
else ifeq ($(CC),clang)
	CFLAGS += -Wpedantic -Wall -Wextra -Werror -Wformat -Wuninitialized -Wshadow -flto=thin
else ifeq ($(CC),zig cc)
	CFLAGS += -flto=thin -Wpedantic -Wall -Wextra -Werror -Wformat -Wuninitialized -Wshadow
else
	CFLAGS += -Wpedantic -Wall -Wextra -Werror -Wformat -Wuninitialized -Wshadow -Wno-stringop-overread
endif

LDFLAGS ?= -Iinclude -Iexternal -Isources

ifeq ($(OS),Windows_NT)
	LDFLAGS += -lwsock32
else
	LDFLAGS += -pthread
endif

OPTIONS = -DPORT=$(PORT) -DAUTHORIZATION=\"$(AUTHORIZATION)\" -DALLOW_UNSECURE_RANDOM=$(ALLOW_UNSECURE_RANDOM) $(if $(CSOCKET_SECURE),-DCSOCKET_SECURE -DCSOCKET_KEY=\"$(CSOCKET_KEY)\" -DCSOCKET_CERT=\"$(CSOCKET_CERT)\",) $(if $(TCPLIMITS_EXPERIMENTAL_SAVE_MEMORY),-DTCPLIMITS_EXPERIMENTAL_SAVE_MEMORY,) -DHARDCODED_SESSION_ID=$(HARDCODED_SESSION_ID) -DGITHUB_COMMIT_SHA=\"$(GITHUB_COMMIT_SHA)\" -DGITHUB_BRANCH=\"$(GITHUB_BRANCH)\" -DPCLL_SSL_LIBRARY=$(PCLL_SSL_LIBRARY) -DAUDIO_QUALITY=$(AUDIO_QUALITY)

SRCS = $(foreach dir,$(SRC_DIR),$(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))

FrequenC: $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(OPTIONS) $(LDFLAGS) $(SSL_FLAGS)

$(OBJ_DIR)/%.o: lib/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS) $(OPTIONS) $(LDFLAGS)

$(OBJ_DIR)/%.o: external/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS) $(OPTIONS) $(LDFLAGS)

$(OBJ_DIR)/%.o: sources/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS) $(OPTIONS) $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $@

debug: CFLAGS=-g -O0 -march=native
debug: FrequenC

.PHONY: clean
clean:
	rm -rf FrequenC $(OBJ_DIR)
