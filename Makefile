CC = clang

# Comment the option below if you want to use experimental mode for saving memory
TCPLIMITS_EXPERIMENTAL_SAVE_MEMORY = 1

# CSOCKET_SECURE = 1
# CSOCKET_KEY = "key.pem"
# CSOCKET_CERT = "cert.pem"
PORT = 8888
AUTHORIZATION = "youshallnotpass"
ALLOW_UNSECURE_RANDOM = 0

SRC_DIR = lib external sources
OBJ_DIR = obj

CVERSION = -std=c99
CFLAGS ?= -Ofast -march=native -fno-signed-zeros -fno-trapping-math -funroll-loops  
LDFLAGS ?= -Iinclude -Iexternal -Isources -pthread
OPTIONS = -DPORT=$(PORT) -DAUTHORIZATION=\"$(AUTHORIZATION)\" -DALLOW_UNSECURE_RANDOM=$(ALLOW_UNSECURE_RANDOM) $(if $(CSOCKET_SECURE),-lssl -lcrypto -DCSOCKET_SECURE -DCSOCKET_KEY=\"$(CSOCKET_KEY)\" -DCSOCKET_CERT=\"$(CSOCKET_CERT)\",) $(if $(TCPLIMITS_EXPERIMENTAL_SAVE_MEMORY),-DTCPLIMITS_EXPERIMENTAL_SAVE_MEMORY,)
CHECK_FLAGS = -Wpedantic -Wall -Wextra -Werror -Wformat -Wuninitialized -Wshadow 

SRCS = $(foreach dir,$(SRC_DIR),$(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))

FrequenC: $(OBJS)
	$(CC) $^ -o $@ $(CVERSION) $(CFLAGS) $(CHECK_FLAGS) $(OPTIONS) $(LDFLAGS) -lssl -lcrypto

$(OBJ_DIR)/%.o: lib/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(CHECK_FLAGS) $(OPTIONS) $(LDFLAGS)

$(OBJ_DIR)/%.o: external/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(CHECK_FLAGS) $(OPTIONS) $(LDFLAGS)

$(OBJ_DIR)/%.o: sources/%.c | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(CHECK_FLAGS) $(OPTIONS) $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf FrequenC $(OBJ_DIR)
