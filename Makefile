BIN=a.out
CC=gcc

SOURCEDIR=src
BUILDDIR=build
SOURCES=$(wildcard $(SOURCEDIR)/*.c)
OBJECTS=$(patsubst $(SOURCEDIR)/%.c, $(BUILDDIR)/%.o, $(SOURCES))

all: $(BIN)

clean:
	rm -rf $(BIN) $(BUILDDIR)

$(BIN): $(BUILDDIR) $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OBJECTS): $(BUILDDIR)/%.o: $(SOURCEDIR)/%.c
	$(CC) -c $< -o $@


TESTDIR=tests
TEST_IN=$(wildcard $(TESTDIR)/*.in)
TEST_OUT=$(wildcard $(TESTDIR)/*.out)
TEST_OBJ=$(patsubst $(TESTDIR)/%.in, $(TESTDIR)/%.test, $(TEST_IN))

test: $(BIN) $(TEST_OBJ)
	@rm -f $(TEST_OBJ)

$(TEST_OBJ): $(TESTDIR)/%.test: $(TESTDIR)/%.in $(TESTDIR)/%.out
	@printf %s "testing $(patsubst $(TESTDIR)/%.test,%, $@) ... "
	@./timer $(BIN) $< $@
	@echo
	@diff $@ $(word 2,$^) && echo '\033[0;32mPASS\033[0m' || echo '\033[0;31mFAIL\033[0m'

