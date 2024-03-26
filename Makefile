FUNC_LATENCY = func_latency
PERF = perf

all: $(PERF) $(FUNC_LATENCY)

# perf tool
kernelversion:
	@echo "6"

$(PERF): 
	$(MAKE) -C tools/perf
	mv tools/perf/$(PERF) ./

# func_latency
SRC_DIR = src
INCLUDE_DIR = -I ./ \
							-I ./include/ 

LIB_PATH = -L ./
LIBS = -lpthread

CXX = g++
# CXXFLAGS = -g -O0 -fsanitize=address -fno-omit-frame-pointer -std=c++14 $(INCLUDE_DIR) $(LIB_PATH) $(LIBS)
CXXFLAGS = -g -O3 -std=c++14 $(INCLUDE_DIR) $(LIB_PATH) $(LIBS)

SRC_FILE = $(wildcard $(SRC_DIR)/*.cc)
OBJS = $(patsubst %.cc,%.o,$(SRC_FILE))

$(FUNC_LATENCY): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ 
	rm -f $(SRC_DIR)/*.o

clean:
	rm -f *.o
	rm -f $(SRC_DIR)/*.o
	$(MAKE) clean -C tools/perf
	rm -f $(PERF) $(FUNC_LATENCY)

.PHONY: kernelversion
