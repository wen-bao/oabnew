CXX = gcc -std=gnu99
INCLUDE = include
BUILD = build
SRC = src
CXXFLAGS = -I$(INCLUDE)

dir_guard=@mkdir -p $(@D)

oabnew: $(BUILD)/oabnew.o $(BUILD)/httpd.o $(BUILD)/slog.o
	$(dir_guard)
	$(CXX) -o $@ $(BUILD)/*.o -Llib -liniparser

$(BUILD)/oabnew.o: $(SRC)/oabnew.c
	$(dir_guard)
	$(CXX) -g $(CXXFLAGS) -c -o $@ $(SRC)/oabnew.c

$(BUILD)/httpd.o: $(SRC)/httpd.c
	$(dir_guard)
	$(CXX) -g $(CXXFLAGS) -c -o $@ $(SRC)/httpd.c

$(BUILD)/slog.o: $(SRC)/slog.c
	$(dir_guard)
	$(CXX) -g $(CXXFLAGS) -c -o $@ $(SRC)/slog.c

all:
	oabnew
clean:
	rm -rf $(BUILD)/* oabnew
