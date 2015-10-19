CC=g++
CFLAGS=-c -Wall
LDFLAGS=-lupm-i2clcd -lupm-grove -lmraa
SOURCES=edison_piano.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=edison_piano

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)
