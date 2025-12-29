#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "token.h"
#include "lexer.h"
#include "lexer_generator.h"

/*
 * 读取整个文件
 */
std::string readFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

int main() {
    try {
        // ===== 读入源代码 =====
        std::string code = readFile("test.txt");

        // ===== 使用规则文件生成扫描器 =====
        LexerGenerator gen;
        // gen.loadRuleFile("rules/expr.lex");
        gen.loadRuleFile("rules/tiny.lex");
        // gen.loadRuleFile("rules/c_like.lex");

        DFA dfa = gen.buildDFA();   // 正则 → NFA → DFA → 最小化 DFA

        // ===== 运行扫描器 =====
        Lexer lexer(code, dfa);

        std::ostringstream output;

        while (true) {
            Token tok = lexer.nextToken();

            if (tok.type == TokenType::ERROR) {
                std::ofstream ofs("output.txt");
                ofs << "Lexical Error: illegal character '"
                    << tok.lexeme << "'\n"
                    << "at line " << tok.line
                    << ", column " << tok.column << "\n";
                return 1;
            }

            output << tokenName(tok.type);

            if (!tok.lexeme.empty()) {
                output << " : " << tok.lexeme;
            }

            output << " (" << tok.line << "," << tok.column << ")";
            output << "\n";


            if (tok.type == TokenType::ENDFILE) {
                break;
            }
        }

        std::ofstream ofs("output.txt");
        ofs << output.str();
    }
    catch (const std::exception& e) {
        std::ofstream ofs("output.txt");
        ofs << "Fatal Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}





// // 最小化前状态数验证
        // // std::cout << "DFA states before minimization: "
        // //   << dfa.states.size() << std::endl;
        // dfa = minimizeDFA(dfa);
        // //最小化后状态数验证
        // // std::cout << "DFA states before minimization: "
        // //   << dfa.states.size() << std::endl;
        // Lexer lexer(code, dfa);