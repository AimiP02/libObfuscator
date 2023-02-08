OPTIMIZER := obfuscator.so

CXX := clang++
CXXFLAGS := $(shell llvm-config --cxxflags) -fPIC -I/home/bronya/work/libObfuscator/include

OPT_OBJs := $(subst .cpp,.o,$(wildcard ./src/*.cpp))
TEST_SRCs := $(basename $(notdir $(wildcard ./tests/*.c)))
TEST_RAW_LLs := $(subst .c,.ll,$(wildcard ./tests/*.c))
TEST_OPT_LLs := $(addprefix ./tests/,$(addsuffix -opt.ll,$(TEST_SRCs)))

LLVM_PATH = $(shell llvm-config --includedir)

all: $(TEST_RAW_LLs) $(TEST_OPT_LLs)

./tests/%-opt.ll: ./tests/%-opt.bc
	llvm-dis $< -o=$@

./tests/%.ll: ./tests/%.bc
	llvm-dis $< -o=$@

./tests/%-opt.bc: ./tests/%.bc $(OPTIMIZER)
	opt -enable-new-pm=1 -load-pass-plugin ./$(OPTIMIZER) -passes=gvobfus $< -o $@

./tests/%.bc: ./tests/%.c
	clang -O0 -Xclang -disable-O0-optnone -emit-llvm -c $< -o $@

$(OPTIMIZER): $(OPT_OBJs)
	$(CXX) -dylib -fPIC -shared $^ -o $@

.PHONY: clean

clean:
	$(RM) tests/linked.bc tests/linked.ll tests/encrypt.ll
	$(RM) $(TEST_OPT_LLs) $(TEST_RAW_LLs) $(OPT_OBJs) $(OPTIMIZER)
