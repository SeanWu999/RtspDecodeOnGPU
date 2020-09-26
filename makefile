#######################
# Makefile
#######################
# source object target
SOURCE = $(wildcard *.cpp)
OBJS   = $(SOURCE:.cpp=.o)
TARGET = build

$(warning $(SOURCE))
$(warning $(OBJS))

# compile and lib parameter
CC       = g++
INCLUDES = -I ./ -I /usr/local/cuda-9.0/include/ -I /usr/local/include/
LIBS    = -lavformat -lavutil -lavcodec -lnvcuvid -lcuda -ldl -lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
LIB_PATH = -L /usr/lib/x86_64-linux-gnu/
CFLAGS = -Wall -shared -fPIC -c 


# link
$(TARGET):$(OBJS)
	$(CC) -std=c++11 $(OBJS) $(LIB_PATH) $(LIBS) -o $(TARGET)

# compile
%.o: %.cpp
	$(CC) -std=c++11 $(INCLUDES) $(LIB_PATH) $(LIBS) $(CFLAGS) $< -o $@  -w

# clean
.PHONY: clean
clean:
	rm -fr *.o
	rm -fr $(TARGET)
