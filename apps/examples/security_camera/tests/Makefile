############################################################################
# tests/Makefile
#
# Host-based unit tests for security camera modules.
# Compiles and runs tests with standard gcc on the development machine,
# not on the NuttX target. NuttX headers are stubbed by mock_nuttx.h.
#
# Usage:
#   make          - build and run all tests
#   make clean    - remove test binaries
#
############################################################################

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -g -I. -I..
CFLAGS  += -include mock_nuttx.h
CFLAGS  += -D_POSIX_C_SOURCE=200809L
CFLAGS  += -Wno-unused-function
LDFLAGS  = -lpthread

TESTS = test_mjpeg_protocol test_frame_queue

.PHONY: all clean test

all: test

test: $(TESTS)
	@echo ""
	@echo "========================================"
	@echo "  Running all tests"
	@echo "========================================"
	@failed=0; \
	for t in $(TESTS); do \
		echo ""; \
		./$$t || failed=1; \
	done; \
	echo ""; \
	echo "========================================"; \
	if [ $$failed -eq 0 ]; then \
		echo "  ALL TEST SUITES PASSED"; \
	else \
		echo "  SOME TESTS FAILED"; \
		exit 1; \
	fi; \
	echo "========================================"

test_mjpeg_protocol: test_mjpeg_protocol.c mock_nuttx.h test_framework.h ../mjpeg_protocol.c ../mjpeg_protocol.h ../config.h
	$(CC) $(CFLAGS) -o $@ test_mjpeg_protocol.c $(LDFLAGS)

test_frame_queue: test_frame_queue.c mock_nuttx.h test_framework.h ../frame_queue.c ../frame_queue.h ../config.h
	$(CC) $(CFLAGS) -o $@ test_frame_queue.c $(LDFLAGS)

clean:
	rm -f $(TESTS)
