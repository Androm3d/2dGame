# Path to the external SOIL folder
SOIL_RELATIVE = ../../external/soil

TARGET = BugsBunnyCrazyCastle
CXX = g++

CXXFLAGS = -std=c++11 -Wall -I. -I/usr/include/freetype2 -I$(SOIL_RELATIVE)/include/SOIL
LDFLAGS = -lGLEW -lglfw -lGL -lGLU -lfreetype -lX11

%.o: %.c
	gcc -I$(SOIL_RELATIVE)/include/SOIL -c $< -o $@

SRCS = $(wildcard *.cpp)
SOIL_SRCS = $(SOIL_RELATIVE)/src/soil.c \
            $(SOIL_RELATIVE)/src/image_dxt.c \
            $(SOIL_RELATIVE)/src/image_helper.c \
            $(SOIL_RELATIVE)/src/stb_image_aug.c

OBJS = $(SRCS:.cpp=.o) $(SOIL_SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# CHANGE: Also point to include/ here for the C compiler
%.o: %.c
	gcc -I$(SOIL_RELATIVE)/include -c $< -o $@

clean:
	rm -f *.o $(TARGET)
	rm -f $(SOIL_RELATIVE)/src/*.o

.PHONY: clean