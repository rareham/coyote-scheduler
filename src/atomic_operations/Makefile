SRC_DIR := .
OBJ_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.cc)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRC_FILES))

CXX := /usr/local/bin/clang++
CXXFLAGS +=  -std=c++17 -g -O0 -lmodel -Wno-unused-command-line-argument -I.

LDFLAGS := -lrt -rdynamic -lpthread -ldl
LDFLAGS := -ldl -lrt -rdynamic -lpthread
SHARED := -shared
LIB_SO := lib_atomic.so

all: $(LIB_SO)

%.o : %.cc
	$(CXX) -MMD -MF .$@.d -fPIC -c $< $(CXXFLAGS)


$(LIB_SO): $(OBJ_FILES)
	$(CXX) $(SHARED) -o $(LIB_SO) $+ $(LDFLAGS)

-include $(OBJ_FILES:%=.%.d)

PHONY += clean
clean:
	rm -f *.o *.so .*.d 

