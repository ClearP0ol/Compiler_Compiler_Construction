#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "token.h"
#include "thompson.h"
#include "dfa.h"
#include "lexer.h"

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

        // ===== 构造 Lexer =====
        State* nfaStart = buildLexerNFA();
        DFA dfa = buildDFA(nfaStart);
        Lexer lexer(code, dfa);

        // ===== 输出缓冲（先不写文件）=====
        std::ostringstream output;

        while (true) {
            Token tok = lexer.nextToken();

            // ===== 词法错误 =====
            if (tok.type == TokenType::ERROR) {
                std::ofstream ofs("output.txt");
                ofs
                    << "Lexical Error: illegal character '"
                    << tok.lexeme << "'\n"
                    << "at line " << tok.line
                    << ", column " << tok.column << "\n";
                ofs.close();
                return 1;
            }

            // ===== 正常 token =====
            output << tokenName(tok.type);
            if (!tok.lexeme.empty()) {
                output << " : " << tok.lexeme;
            }
            output << "\n";

            if (tok.type == TokenType::ENDFILE) {
                break;
            }
        }

        // ===== 没有错误：写出全部 token =====
        std::ofstream ofs("output.txt");
        ofs << output.str();
        ofs.close();
    }
    catch (const std::exception& e) {
        std::ofstream ofs("output.txt");
        ofs << "Fatal Error: " << e.what() << "\n";
        ofs.close();
        return 1;
    }

    return 0;
}
