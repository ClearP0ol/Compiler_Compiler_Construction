# ===== Compiler Driver Makefile =====

CXX := g++
CXXFLAGS := -std=c++17 -Wall -O2

TARGET := compiler.exe
SRC := main.cpp

# ===== build =====
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# ===== run example =====
# make run SRC=../test/test.tiny LEX=../rules/tiny.lex GRAM=../rules/tiny.grammar
run:
	./$(TARGET) $(SRC) $(LEX) $(GRAM)

# ===== clean =====
clean:
	del /Q $(TARGET) 2>nul || true
