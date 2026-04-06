# Makefile — libantheos C++17
#
# Builds libantheos.a and test binaries.
# Usage:
#   make              Build library
#   make test         Build and run all test suites
#   make install      Install library and header (sudo)
#   make uninstall    Remove installed files (sudo)
#   make clean        Remove build artifacts
#   make help         Show available targets

PREFIX   = /usr/local
LIBDIR   = $(PREFIX)/lib
INCDIR   = $(PREFIX)/include/antheos

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -O2 -fPIC -Iinclude
ARFLAGS  = rcs

SRCDIR   = src
BUILDDIR = build
TESTDIR  = tests

# ── Library ──

SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
LIB  = $(BUILDDIR)/libantheos.a
SO   = $(BUILDDIR)/libantheos.so

# ── Test binaries ──

TEST_WIRE        = $(BUILDDIR)/test_wire
TEST_PARSER      = $(BUILDDIR)/test_parser
TEST_IDENTITY    = $(BUILDDIR)/test_identity
TEST_BUS         = $(BUILDDIR)/test_bus
TEST_SERVICE     = $(BUILDDIR)/test_service
TEST_SESSION     = $(BUILDDIR)/test_session
TEST_CONFORMANCE = $(BUILDDIR)/test_conformance
TEST_EDGE        = $(BUILDDIR)/test_edge
TEST_CONTEXT     = $(BUILDDIR)/test_context
TEST_DEPTH       = $(BUILDDIR)/test_depth

TESTS = $(TEST_WIRE) $(TEST_PARSER) $(TEST_IDENTITY) $(TEST_BUS) $(TEST_SERVICE) $(TEST_SESSION) $(TEST_CONFORMANCE) $(TEST_EDGE) $(TEST_CONTEXT) $(TEST_DEPTH)

# ── Targets ──

.PHONY: all clean test install uninstall help

all: $(LIB) $(SO)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp include/antheos.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(LIB): $(OBJS)
	ar $(ARFLAGS) $@ $^

$(SO): $(OBJS)
	$(CXX) -shared -o $@ $^

# ── Test build rules ──

$(TEST_WIRE): $(TESTDIR)/test_wire.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_wire_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_PARSER): $(TESTDIR)/test_parser.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_parser_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_IDENTITY): $(TESTDIR)/test_identity.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_identity_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_BUS): $(TESTDIR)/test_bus.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_bus_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_SERVICE): $(TESTDIR)/test_service.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_service_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_SESSION): $(TESTDIR)/test_session.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_session_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_CONFORMANCE): $(TESTDIR)/test_conformance.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_conformance_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_EDGE): $(TESTDIR)/test_edge.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_edge_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_CONTEXT): $(TESTDIR)/test_context.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_context_run -o $@ $< -L$(BUILDDIR) -lantheos

$(TEST_DEPTH): $(TESTDIR)/test_depth.cpp $(LIB) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(TESTDIR) -DTEST_MAIN_FN=test_depth_run -o $@ $< -L$(BUILDDIR) -lantheos

# ── Test runner ──

test: $(TESTS)
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t || exit 1; done

test_wire: $(TEST_WIRE)
	./$(TEST_WIRE)

test_parser: $(TEST_PARSER)
	./$(TEST_PARSER)

test_identity: $(TEST_IDENTITY)
	./$(TEST_IDENTITY)

test_bus: $(TEST_BUS)
	./$(TEST_BUS)

test_service: $(TEST_SERVICE)
	./$(TEST_SERVICE)

test_session: $(TEST_SESSION)
	./$(TEST_SESSION)

test_conformance: $(TEST_CONFORMANCE)
	./$(TEST_CONFORMANCE)

test_edge: $(TEST_EDGE)
	./$(TEST_EDGE)

test_context: $(TEST_CONTEXT)
	./$(TEST_CONTEXT)

test_depth: $(TEST_DEPTH)
	./$(TEST_DEPTH)

# ── Install / Uninstall ──

install: $(LIB) $(SO)
	install -d $(LIBDIR)
	install -m 644 $(LIB) $(LIBDIR)/libantheos.a
	install -m 755 $(SO) $(LIBDIR)/libantheos.so
	install -d $(INCDIR)
	install -m 644 include/antheos.hpp $(INCDIR)/
	ldconfig
	@echo "libantheos installed to $(LIBDIR)/"
	@echo "Header installed to $(INCDIR)/"

uninstall:
	rm -f $(LIBDIR)/libantheos.a $(LIBDIR)/libantheos.so
	rm -rf $(INCDIR)
	ldconfig
	@echo "libantheos uninstalled"

# ── Clean ──

clean:
	rm -rf $(BUILDDIR)

# ── Help ──

help:
	@echo "libantheos C++17 — available targets:"
	@echo "  all          Build libantheos.a (default)"
	@echo "  test         Build and run all test suites"
	@echo "  test_wire    Run wire encoding tests"
	@echo "  install      Install library and header to PREFIX (default: /usr/local)"
	@echo "  uninstall    Remove installed library and header"
	@echo "  clean        Remove build artifacts"
	@echo "  help         Show this message"
