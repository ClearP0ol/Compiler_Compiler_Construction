#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

void dumpFile(const std::string& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::cerr << "[Compiler] Cannot open " << path << "\n";
        return;
    }
    std::string line;
    while (std::getline(fin, line)) {
        std::cerr << line << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr
            << "Usage:\n"
            << "  compiler <source.txt> <lexer.lex> <parser.grammar>\n";
        return 1;
    }

    std::string source  = argv[1];
    std::string lex     = argv[2];
    std::string grammar = argv[3];

    // lexer 实际生成的 token 文件（相对于 Compiler/）
    std::string tokenFile = "output.txt";

    // ===== 1. 运行词法分析器 =====
    std::string lexerCmd =
        "..\\Lexical_analyzer\\lexer_gen.exe " +
        source + " " + lex;

    std::cout << "[Compiler] Running lexer:\n  " << lexerCmd << "\n";
    int ret = std::system(lexerCmd.c_str());
    if (ret != 0) {
        std::cerr << "[Compiler] Lexer failed. Dumping output.txt for diagnosis:\n";
        dumpFile(tokenFile);
        return 1;
    }

    // ===== 2. 运行语法分析器 =====
    std::string parserCmd =
        "..\\Syntactic_analyzer\\SyntacticAnalyzer.exe " +
        grammar + " " + tokenFile;

    std::cout << "[Compiler] Running parser:\n  " << parserCmd << "\n";
    ret = std::system(parserCmd.c_str());
    if (ret != 0) {
        std::cerr << "[Compiler] Parser failed.\n";
        return 1;
    }

    return 0;
}
