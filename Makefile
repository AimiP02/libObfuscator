OPTIMIZER := obfuscator.so

CXXFLAGS := $(shell llvm-config --cxxflags) -I../../include -fPIC

OPT_OBJs := $(subst .cpp,.o,$(wildcard ./src/*.cpp))
TEST_SRCs := $(basename $(notdir $(wildcard ./tests/*.c)))
TEST_RAW_LLs := $(subst .c,.ll,$(wildcard ./tests/*.c))
TEST_OPT_LLs := $(addprefix ./tests/,$(addsuffix -opt.ll,$(TEST_SRCs)))

LLVM_CODE_INCLUDE_PATH := /home/bronya/developer/llvm-12.0.1/llvm/include
LLVM_HOME_INCLUDE_PATH := /home/bronya/developer/llvm-12.0.1/build/include

all: $(TEST_RAW_LLs)

# ./tests/%-opt.ll: ./tests/%-opt.bc
# 	llvm-dis $< -o=$@

./tests/%.ll: ./tests/%-m2r.bc
	llvm-dis $< -o=$@

# ./tests/%-opt.bc: ./tests/%-m2r.bc $(OPTIMIZER)
# 	env LD_LIBRARY_PATH=. opt -load-pass-plugin $(OPTIMIZER) -passes=obfuscator $< -o $@

./tests/%-m2r.bc: ./tests/%.bc
	opt -mem2reg $< -o $@

./tests/%.bc: ./tests/%.c
	clang -O0 -Xclang -disable-O0-optnone -emit-llvm -c -I$(LLVM_CODE_INCLUDE_PATH) -I$(LLVM_HOME_INCLUDE_PATH) $< -o $@

$(OPTIMIZER): $(OPT_OBJs)
	clang++ -dylib -fPIC -shared -I$(LLVM_CODE_INCLUDE_PATH) -I$(LLVM_HOME_INCLUDE_PATH) $^ -o $@

.PHONY: clean

clean:
	rm $(TEST_OPT_LLs) $(TEST_RAW_LLs) $(OPT_OBJs) $(OPTIMIZER)
