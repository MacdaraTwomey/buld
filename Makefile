.SUFFIXES:
.DELETE_ON_ERROR:
MAKEFLAGS=--warn-undefined-variables

all: 
	bear --append -- $(MAKE) everything

.PHONY: everything
everything: build/buld

.PHONY: clean
clean: 
	rm -rf build

CC_FLAGS := -g -std=c++20 -fsanitize=address -MD -Wall -pedantic-errors \
	-Wno-unused-variable -Wno-gnu-anonymous-struct -Wno-writable-strings \
	-Wno-nested-anon-types -Wno-gnu-zero-variadic-macro-arguments	-Wno-missing-braces \
	-fno-strict-aliasing -Wno-unused-function -Wno-language-extension-token \
	# -fdiagnostics-color    # forces colour even when piping to sed

CC_FLAGS += -fdiagnostics-absolute-paths

LINK_FLAGS := -fsanitize=address -fuse-ld=lld 


INC := 
SRCS := $(wildcard src/*.cpp)
#OBJS := $(patsubst build/%.o,%.c,$(SRCS))
OBJS := $(SRCS:src/%.cpp=build/%.o)

build/%.o: src/%.cpp | build
	clang++ -o $@ $(CC_FLAGS) $(INC) -c $<

build/buld: $(OBJS) | build
	clang++ -o $@ $(LINK_FLAGS) $^

build: 
	mkdir -p build

DEPS := $(wildcard build/*.d)
-include $(DEPS)
