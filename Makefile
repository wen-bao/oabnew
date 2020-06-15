CXX = gcc -std=gnu99
INCLUDE = include
BIN = bin
BUILD = build
SRC = src
CXXFLAGS = -I$(INCLUDE) -L$(BIN)

dir_guard=@mkdir -p $(@D)

$(BIN)/oabnew: $(BUILD)/oabnew.o $(BUILD)/httpd.o $(BUILD)/slog.o
	$(dir_guard)
	$(CXX) -o $@ $(BUILD)/*.o

$(BUILD)/oabnew.o: oabnew.c
	$(dir_guard)
	$(CXX) $(CXXFLAGS) -c -o $@ oabnew.c

$(BUILD)/httpd.o: $(SRC)/httpd.c
	$(dir_guard)
	$(CXX) $(CXXFLAGS) -c -o $@ $(SRC)/httpd.c

$(BUILD)/slog.o: $(SRC)/slog.c
	$(dir_guard)
	$(CXX) $(CXXFLAGS) -c -o $@ $(SRC)/slog.c

all:
	$(BIN)/oabnew
clean:
	rm -rf $(BUILD)/* $(BIN)/oabnew
