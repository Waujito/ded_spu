BUILD_DIR := build

STATIC_LIB := $(BUILD_DIR)/tasks_lib.a
STATIC_LIB_TARGET := deps/ded_tasks

SANITIZER_FLAGS := -fsanitize=address,alignment,bool,bounds,enum,float-cast-overflow,float-divide-by-zero,integer-divide-by-zero,leak,nonnull-attribute,null,object-size,return,returns-nonnull-attribute,shift,signed-integer-overflow,undefined,unreachable,vla-bound,vptr

CFLAGS := -D _DEBUG -ggdb3 -O0 -Wall -Wextra -Waggressive-loop-optimizations -Wmissing-declarations -Wcast-align -Wcast-qual -Wchar-subscripts  -Wconversion -Wempty-body -Wfloat-equal -Wformat-nonliteral -Wformat-security -Wformat-signedness -Wformat=2 -Winline -Wlogical-op -Wopenmp-simd -Wpacked -Wpointer-arith -Winit-self -Wredundant-decls -Wshadow -Wsign-conversion -Wstrict-overflow=2 -Wsuggest-attribute=noreturn -Wsuggest-final-methods -Wsuggest-final-types -Wswitch-default -Wswitch-enum -Wsync-nand -Wundef -Wunreachable-code -Wunused -Wuseless-cast -Wvariadic-macros -Wno-missing-field-initializers -Wno-narrowing -Wno-varargs -Wstack-protector -fcheck-new -fstack-protector -fstrict-overflow -fno-omit-frame-pointer -Wlarger-than=8192 -Wstack-usage=8192 -pie -fPIE -Werror=vla -Iinclude -D _GNU_SOURCE $(SANITIZER_FLAGS) -I$(STATIC_LIB_TARGET)/include

ifdef USE_GTEST
override CFLAGS += -DUSE_GTEST
endif

CXXFLAGS := $(CFLAGS) -Weffc++ -Wc++14-compat -Wconditionally-supported -Wctor-dtor-privacy -Wnon-virtual-dtor -Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel -Wsuggest-override -Wno-literal-suffix -Wno-old-style-cast -std=c++17 -fsized-deallocation

CXX := g++
CC := gcc
FLAGS = $(CXXFLAGS)

LDFLAGS := -lm

# Uncomment next two lines for C compiler
# OBJCFLAGS := -xc -std=c11
# FLAGS := $(CFLAGS)
# CXX := $(CC)

# -flto-odr-type-merging

TESTLIBSRC := test/test_runner.cpp
TESTLIBOBJ := $(TESTLIBSRC:%.cpp=$(BUILD_DIR)/%.o)

TESTSRC := test/test_pvector.cpp test/test_cppvector.cpp test/test_crc32.cpp
TESTOBJ := $(TESTSRC:%.cpp=$(BUILD_DIR)/%.o)
TEST_LIB_APP := $(BUILD_DIR)/test_spu

TRANSLATOR_SRC := src/translator/translator.cpp# src/translator/address.cpp
TRANSLATOR_OBJ := $(TRANSLATOR_SRC:%.cpp=$(BUILD_DIR)/%.o)
TRANSLATOR_APP := $(BUILD_DIR)/translator

SPU_SRC := src/spu/spu.cpp
SPU_OBJ := $(SPU_SRC:%.cpp=$(BUILD_DIR)/%.o)
SPU_APP := $(BUILD_DIR)/spu

INCPDSRC := $(SPU_SRC) $(TRANSLATOR_SRC) $(TESTSRC) $(LIBSRC) $(TESTLIBSRC)
incpd := $(INCPDSRC:%.cpp=$(BUILD_DIR)/%.d)

OBJFILES := $(LIBOBJ) $(TESTOBJ) $(TRANSLATOR_OBJ) $(SPU_OBJ) $(TESTLIBOBJ)
OBJDIRS := $(sort $(dir $(OBJFILES)))

define INCFIRE
	@echo IN CASE OF FIRE
	@echo GIT COMMIT
	@echo GIT PUSH
	@echo MEOW
	@echo LEAVE BUILDING
	@echo THE TARGET IS BUILT SUCCSESSFULLY
	@echo
endef

.PHONY: build clean run test document build_test objdirs

build: $(SPU_APP) $(TRANSLATOR_APP) $(STATIC_LIB)
	$(INCFIRE)

spu: $(SPU_APP)
	./$(SPU_APP)

translator: $(TRANSLATOR_APP)
	./$(TRANSLATOR_APP)

$(OBJDIRS):
	mkdir -p $(OBJDIRS)

$(OBJFILES): $(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(OBJDIRS)
	# $< takes only the FIRST dependency
	$(CXX) $(FLAGS) $(OBJCFLAGS) -MP -MMD -c $< -o $@

$(STATIC_LIB): $(OBJDIRS)
	$(MAKE) -C $(STATIC_LIB_TARGET)
	cp $(STATIC_LIB_TARGET)/build/tasks_lib.a $(STATIC_LIB)

ifdef USE_GTEST
$(TEST_LIB_APP): $(STATIC_LIB) $(TESTOBJ)
	$(CXX) $(FLAGS) $(LDFLAGS) $(TESTOBJ) $(STATIC_LIB) -lgtest_main -lgtest -o $(TEST_LIB_APP)
else
$(TEST_LIB_APP): $(STATIC_LIB) $(TESTOBJ) $(TESTLIBOBJ)
	$(CXX) $(FLAGS) $(LDFLAGS) $(TESTOBJ) $(TESTLIBOBJ) $(STATIC_LIB) -o $(TEST_LIB_APP)
endif


build_test: $(TEST_LIB_APP)
	$(INCFIRE)

test: build_test
	./$(TEST_LIB_APP)

$(TRANSLATOR_APP): $(TRANSLATOR_OBJ) $(STATIC_LIB)
	$(CXX) $(FLAGS) $(LDFLAGS) $(TRANSLATOR_OBJ) $(STATIC_LIB) -o $@ 

$(SPU_APP): $(SPU_OBJ) $(STATIC_LIB)
	$(CXX) $(FLAGS) $(LDFLAGS) $(SPU_OBJ) $(STATIC_LIB) -o $@

document: objdirs
	doxygen doxygen.conf

clean:
	rm -rf build

distclean: clean
	$(MAKE) -C $(STATIC_LIB_TARGET) clean

-include $(incpd)
