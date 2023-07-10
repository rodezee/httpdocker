# Variables
PROG ?= httpdocker                # Program we are building
ARGS ?= -d /www                   # serve /www
DELETE = rm -rf                   # Command to remove files
OUT ?= -o $(PROG)                 # Compiler argument for output file

# Source code files
SOURCES = src/main.c src/lib/mongoose/mongoose.c src/lib/libdocker/src/docker.c src/mgoverride.c
CFLAGS = -W -Wall -Wextra -g -lcurl -I.  # Build options
CFLAGS_EXTRA = -DMGOVERRIDE       # mg override function

# Mongoose build options. See https://mongoose.ws/documentation/#build-options
CFLAGS_MONGOOSE += -DMG_HTTP_DIRLIST_TIME_FMT="%Y/%m/%d %H:%M:%S"
CFLAGS_MONGOOSE += -DMG_ENABLE_LINES=1 -DMG_ENABLE_IPV6=1 -DMG_ENABLE_SSI=1

ifeq ($(OS),Windows_NT)         # Windows settings. Assume MinGW compiler. To use VC: make CC=cl CFLAGS=/MD OUT=/Feprog.exe
  PROG ?= $(PROG).exe           # Use .exe suffix for the binary
  CC = gcc                      # Use MinGW gcc compiler
  CFLAGS += -lws2_32            # Link against Winsock library
  DELETE = cmd /C del /Q /F /S  # Command prompt command to delete files
  OUT ?= -o $(PROG)             # Build output
endif

all: build run            # Default target. Build and run program

build: $(SOURCES)             # Build program from sources
	$(CC) $(SOURCES) $(CFLAGS) $(CFLAGS_MONGOOSE) $(CFLAGS_EXTRA) $(OUT)

run:                      # Run program
	./$(PROG) $(ARGS)

clean:                        # Cleanup. Delete built program and all build artifacts
	$(DELETE) $(PROG) *.o *.obj *.exe *.dSYM
