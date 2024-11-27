# target
FUNC_LATENCY = func_latency
PERF = perf
PERF_DLFILTER = perf_dlfilter.so

# config
PREFIX = /usr/share/pt_perf
DEBUG = 0
INSTALL = install
ECHO = echo

all: $(PERF) $(PERF_DLFILTER) $(FUNC_LATENCY)

# perf tool
kernelversion:
	@echo "6"

PERF_DIR = tools/perf

$(PERF): 
	$(MAKE) -C $(PERF_DIR) DEBUG=$(DEBUG)
	mv $(PERF_DIR)/$(PERF) ./

# func_latency
SRC_DIR = src
INCLUDE_DIR = -I ./ \
							-I ./include/ 

LIB_PATH = -L ./
LIBS = -lpthread

CXX = g++
# CXXFLAGS = -g -O0 -fsanitize=address -fno-omit-frame-pointer -std=c++14 $(INCLUDE_DIR) $(LIB_PATH) $(LIBS)
ifeq ($(DEBUG), 1)
CXXFLAGS = -g -O0 -std=c++17 $(INCLUDE_DIR) $(LIB_PATH) $(LIBS) -DDEBUG
else
CXXFLAGS = -g -O3 -std=c++17 $(INCLUDE_DIR) $(LIB_PATH) $(LIBS)
endif

SRC_FILE = $(SRC_DIR)/func_latency.cc \
           $(SRC_DIR)/stat_tools.cc   \
           $(SRC_DIR)/sys_tools.cc    \
           $(SRC_DIR)/pt_action.cc    \
           $(SRC_DIR)/pt_linux_perf.cc    \
           $(SRC_DIR)/worker.cc
OBJS = $(patsubst %.cc,%.o,$(SRC_FILE))

$(FUNC_LATENCY): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ 
	rm -f $(SRC_DIR)/*.o

$(PERF_DLFILTER):
	$(CXX) $(CXXFLAGS) -shared -fPIC -o $@ $(SRC_DIR)/perf_dlfilter.cc

clean:
	rm -f *.o
	rm -f $(SRC_DIR)/*.o
	$(MAKE) clean -C tools/perf
	rm -f $(PERF) $(PERF_DLFILTER) $(FUNC_LATENCY)

install:
	@${INSTALL} -d -m 755 ${PREFIX}
	@${ECHO} 'INSTALL' ${FUNC_LATENCY} 'to' ${PREFIX}
	@${INSTALL} ${FUNC_LATENCY} ${PREFIX}/
	@${ECHO} 'INSTALL' ${PERF} 'to' ${PREFIX}
	@${INSTALL} ${PERF} ${PREFIX}/
	@${ECHO} 'INSTALL' ${PERF_DLFILTER} 'to' ${PREFIX}
	@${INSTALL} ${PERF_DLFILTER} ${PREFIX}/
	@${ECHO} 'INSTALL scripts to' ${PREFIX}
	@${INSTALL} -d -m 755 ${PREFIX}/scripts
	@${INSTALL} scripts/* ${PREFIX}/scripts/

.PHONY: kernelversion $(PERF) install
