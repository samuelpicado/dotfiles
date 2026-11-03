CXXFLAGS=-shared -fPIC --no-gnu-unique -Wall -Wextra -DWLR_USE_UNSTABLE -std=c++23 -O2
INCLUDES = `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon lua`
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
TARGET = Hyprspace.so
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -shared -fPIC --no-gnu-unique $(OBJ) -o $@

src/%.o: src/%.cpp $(wildcard src/*.hpp)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

install:
	install -D -m 0755 $(TARGET) $(DESTDIR)$(LIBDIR)/$(TARGET)

clean:
	rm ./$(TARGET)

.PHONY: all test
qa-gesture-driver:
	$(CXX) $(CXXFLAGS) $(INCLUDES) tests/gesture_driver.cpp -o GestureDriver.so

test:
	$(CXX) -std=c++23 -Wall -Wextra -Werror -Isrc src/Model.cpp tests/model_test.cpp -o /tmp/cascade-model-test
	/tmp/cascade-model-test
	$(CXX) -std=c++23 -Wall -Wextra -Werror -Isrc tests/layout_test.cpp -o /tmp/cascade-layout-test
	/tmp/cascade-layout-test

withhyprpmheaders: export PKG_CONFIG_PATH = $(XDG_DATA_HOME)/hyprpm/headersRoot/share/pkgconfig
withhyprpmheaders: all
