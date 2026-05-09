CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS  = -lncurses

TARGET  = kse_dashboard
SRCS    = main.cpp \
          DataLoader.cpp \
          Analytics.cpp \
          CoordinateCompressor.cpp \
          PersistentSegmentTree.cpp

OBJS    = $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) KSE100-20years.csv

clean:
	rm -f $(OBJS) $(TARGET)
