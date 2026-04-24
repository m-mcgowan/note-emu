
# note-emu native demo — build with libcurl
#
# Usage:
#   make                  Build the demo
#   make run              Build and run (requires NOTEHUB_PAT env var)
#   make clean            Remove build artifacts

CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -O2
SRCDIR    = ../../src

CURL_CFLAGS := $(shell curl-config --cflags 2>/dev/null)
CURL_LIBS   := $(shell curl-config --libs 2>/dev/null)

SRCS = main.c $(SRCDIR)/note/emu/emu.c $(SRCDIR)/note/emu/curl.c
TARGET = note-emu-demo

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(SRCDIR)/note/emu/emu.h $(SRCDIR)/note/emu/curl.h
	$(CC) $(CFLAGS) -I$(SRCDIR) $(CURL_CFLAGS) -o $@ $(SRCS) $(CURL_LIBS)

run: $(TARGET)
	@if [ -z "$$NOTEHUB_PAT" ]; then \
		echo "error: NOTEHUB_PAT environment variable is required"; \
		exit 1; \
	fi
	./$(TARGET)

clean:
	rm -f $(TARGET)
