# ==============================================================================
# Makefile: Optimal Delivery Route Planning for E-Commerce Applications (DAA)
# Supports Windows (MinGW/MSYS2/UCRT64) and Linux/macOS
# ==============================================================================

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -ICommon -Ibackend -DSQLITE_THREADSAFE=1

# Directories
COMMON_DIR = Common
CONSOLE_DIR = Console
BACKEND_DIR = backend
TEST_DIR = tests
BIN_DIR = bin
DB_DIR = database

# Common DAA Algorithm Sources
COMMON_SRCS = $(COMMON_DIR)/route.c \
              $(COMMON_DIR)/nearest_neighbor.c \
              $(COMMON_DIR)/branch_bound.c

# Backend HTTP Server Sources
SERVER_SRCS = $(BACKEND_DIR)/server.c \
              $(BACKEND_DIR)/database.c \
              $(BACKEND_DIR)/geocoding.c \
              $(BACKEND_DIR)/routing.c \
              $(BACKEND_DIR)/http_client.c \
              $(BACKEND_DIR)/cJSON.c \
              $(COMMON_SRCS)

# Console and Test Sources
CONSOLE_SRCS = $(CONSOLE_DIR)/main.c $(COMMON_SRCS)
TEST_SRCS = $(TEST_DIR)/verify_all.c $(COMMON_SRCS)

# OS Specific Binary Extensions and Libraries
ifeq ($(OS),Windows_NT)
    SERVER_BIN = $(BIN_DIR)/delivery_server.exe
    CONSOLE_BIN = $(BIN_DIR)/delivery_planner_console.exe
    TEST_BIN = $(BIN_DIR)/verify_all.exe
    SQLITE_OBJ = $(BIN_DIR)/sqlite3.o
    LIBS = -lws2_32
    MKDIR_P = if not exist $(BIN_DIR) mkdir $(BIN_DIR)
    MKDIR_DB = if not exist $(DB_DIR) mkdir $(DB_DIR)
    CLEAN_CMD = del /Q /F $(BIN_DIR)\*.exe $(BIN_DIR)\*.o 2>nul || exit 0
else
    SERVER_BIN = $(BIN_DIR)/delivery_server
    CONSOLE_BIN = $(BIN_DIR)/delivery_planner_console
    TEST_BIN = $(BIN_DIR)/verify_all
    SQLITE_OBJ = $(BIN_DIR)/sqlite3.o
    LIBS = -lpthread -ldl -lm
    MKDIR_P = mkdir -p $(BIN_DIR)
    MKDIR_DB = mkdir -p $(DB_DIR)
    CLEAN_CMD = rm -f $(BIN_DIR)/* $(BACKEND_DIR)/*.o
endif

.PHONY: all server console test clean help run

all: server console

$(BIN_DIR):
	@$(MKDIR_P)
	@$(MKDIR_DB)

$(SQLITE_OBJ): $(BACKEND_DIR)/sqlite3.c $(BACKEND_DIR)/sqlite3.h
	@echo Compiling SQLite engine object: $(SQLITE_OBJ) ...
	$(CC) -c -O2 $(BACKEND_DIR)/sqlite3.c -o $(SQLITE_OBJ) -DSQLITE_THREADSAFE=1

# Compile Web Application C Backend Server
server: $(BIN_DIR) $(SQLITE_OBJ) $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(SQLITE_OBJ) -o $(SERVER_BIN) $(LIBS)
	@echo [SUCCESS] C HTTP Backend built: $(SERVER_BIN)

# Compile Console Application
console: $(BIN_DIR) $(CONSOLE_SRCS)
	$(CC) $(CFLAGS) $(CONSOLE_SRCS) -o $(CONSOLE_BIN)
	@echo [SUCCESS] Console Application built: $(CONSOLE_BIN)

# Compile and Run Automated Verification Suite
test: $(BIN_DIR) $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o $(TEST_BIN)
	@echo [SUCCESS] Test Suite built: $(TEST_BIN)
	@echo Running automated verification suite...
	@$(TEST_BIN)

# Start Web Application Server
run: server
	@echo Starting C HTTP Backend on port 8080...
	@echo Open http://localhost:8080 in your web browser.
	@$(SERVER_BIN)

clean:
	@$(CLEAN_CMD)
	@echo [CLEAN] Removed binaries and objects.

help:
	@echo Optimal Delivery Route Planning Build System:
	@echo   make all      - Compiles both Web Server and Console Application
	@echo   make server   - Compiles the C HTTP Backend Server
	@echo   make console  - Compiles the Console Application
	@echo   make test     - Compiles and runs the full verification test suite
	@echo   make run      - Compiles and launches the Web Application Server
	@echo   make clean    - Removes built executables
