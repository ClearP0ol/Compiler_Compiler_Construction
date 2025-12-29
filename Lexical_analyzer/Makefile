CXX = g++
CXXFLAGS = -std=c++17 -Wall

INCLUDES = -Iautomata -Igenerator -Iruntime -Itoken

SRC = main.cpp \
      automata/nfa.cpp \
      automata/thompson.cpp \
      automata/dfa.cpp \
      automata/dfa_min.cpp \
      generator/lexer_generator.cpp \
      generator/lexer_rule_parser.cpp \
      runtime/lexer.cpp

TARGET = lexer_gen

all: $(TARGET)

$(TARGET):
	$(CXX) $(CXXFLAGS) $(SRC) $(INCLUDES) -o $(TARGET)

clean:
	rm -f $(TARGET)
