TARGET = func_latency
SRC_DIR = src
INCLUDE_DIR = -I ./ \
							-I ./include/ 

LIB_PATH = -L ./
LIBS = 

CXX = g++
CXXFLAGS = -g -O0 -std=c++11 $(INCLUDE_DIR) $(LIB_PATH) $(LIBS)

SRC_FILE = $(wildcard $(SRC_DIR)/*.cc)
OBJS = $(patsubst %.cc,%.o,$(SRC_FILE))

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

all: $(TARGET)
	rm -f $(src)

clean:
	rm -f *.o
	rm -f $(SRC_DIR)/*.o 
