CXX       := gcc
CFLAGS	:= -Wall -Isrc

BIN     := bin
SRC     := src
INCLUDE := include
LIB     := lib
LIBRARIES   := 
EXECUTABLE  := main
DEBUG := debug

all:$(EXECUTABLE) $(BIN)/$(EXECUTABLE) $(DEBUG)/$(EXECUTABLE)

run: clean all
	./$(BIN)/$(EXECUTABLE)

$(EXECUTABLE): $(SRC)/*.c 
	$(CXX) $(CFLAGS) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

$(BIN)/$(EXECUTABLE): $(SRC)/*.c 
	$(CXX) $(CFLAGS) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

$(DEBUG)/$(EXECUTABLE): $(SRC)/*.c 
	$(CXX) $(CFLAGS) -g -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
	@echo "🧹 Clearing..."
	-rm -rf $(BIN)/*
	-rm -rf $(DEBUG)/*
	-rm -rf tmp/*
	-rm -rf output/*
	-rm -rf build/*