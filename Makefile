CXX ?= g++
CXXFLAGS ?= -shared -fPIC --no-gnu-unique -g -std=c++2b $(shell pkg-config --cflags hyprland pixman-1 libdrm)
LDFLAGS ?=

all:
	$(CXX) $(CXXFLAGS) -o hypr-dwindle-solo.so main.cpp soloCenter.cpp $(LDFLAGS)

clean:
	rm -f hypr-dwindle-solo.so
