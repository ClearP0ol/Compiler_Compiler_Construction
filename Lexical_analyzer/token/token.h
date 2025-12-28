#pragma once
#include <string>
#include <stdexcept>


/*
 * TokenType
 * =========
 * 类 C 语言「最小可编译子集」的核心 Token 集
 * Lexer / Parser / IR / CodeGen 都依赖这里
 */
enum class TokenType {
    // ===== 控制 / 结束 =====
    ENDFILE,        // 文件结束
    ERROR,      // 词法错误

    // ===== Tiny 专用 =====
    READ,
    WRITE,

    // ===== 标识符 & 常量 =====
    ID,         // 标识符
    NUM,        // 整数常量

    // ===== 关键字 =====
    INT,     // int
    VOID,    // void
    IF,      // if
    ELSE,    // else
    WHILE,   // while
    RETURN,  // return

    // ===== 运算符 =====
    ASSIGN,     // =
    PLUS,       // +
    MINUS,      // -
    MULT,        // *
    DIV,        // /

    // ===== 关系运算符 =====
    LT,         // <
    GT,         // >
    LTE,         // <=
    GTE,         // >=
    EQ,         // ==
    NEQ,         // !=

    // ===== 界符 =====
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,     // {
    RBRACE,     // }
    SEMI,  // ;
    COMMA       // ,
};

/*
 * Token
 * =====
 * Lexer 的输出单位
 */
struct Token {
    TokenType type;
    std::string lexeme; // 原始字符串（用于符号表 / 报错）
    int line;
    int column;
};

inline int tokenPriority(TokenType t) {
    switch (t) {
        // ===== 关键字（最高优先级）=====
        case TokenType::INT:
        case TokenType::VOID:
        case TokenType::IF:
        case TokenType::ELSE:
        case TokenType::WHILE:
        case TokenType::RETURN:
        case TokenType::WRITE:
        case TokenType::READ:
            return 1;

        // ===== 标识符 =====
        case TokenType::ID:
            return 2;

        // ===== 常量 =====
        case TokenType::NUM:
            return 3;

        // ===== 其他 =====
        default:
            return 10;
    }
}


/*
 * tokenName
 * =========
 * 调试 / 打印用，不参与语义
 */
inline std::string tokenName(TokenType t) {
    switch (t) {
        case TokenType::ENDFILE:        return "ENDFILE";
        case TokenType::ERROR:      return "ERROR";

        case TokenType::ID:         return "ID";
        case TokenType::NUM:        return "NUM";

        case TokenType::INT:     return "INT";
        case TokenType::VOID:    return "VOID";
        case TokenType::IF:      return "IF";
        case TokenType::ELSE:    return "ELSE";
        case TokenType::WHILE:   return "WHILE";
        case TokenType::RETURN:  return "RETURN";

        case TokenType::ASSIGN:     return "ASSIGN";
        case TokenType::PLUS:       return "PLUS";
        case TokenType::MINUS:      return "MINUS";
        case TokenType::MULT:        return "MULT";
        case TokenType::DIV:        return "DIV";

        case TokenType::LT:         return "LT";
        case TokenType::GT:         return "GT";
        case TokenType::LTE:         return "LTE";
        case TokenType::GTE:         return "GTE";
        case TokenType::EQ:         return "EQ";
        case TokenType::NEQ:         return "NEQ";

        case TokenType::LPAREN:         return "LPAREN";
        case TokenType::RPAREN:         return "RPAREN";
        case TokenType::LBRACE:     return "LBRACE";
        case TokenType::RBRACE:     return "RBRACE";
        case TokenType::SEMI:  return "SEMI";
        case TokenType::COMMA:      return "COMMA";
        case TokenType::READ:      return "READ";
        case TokenType::WRITE:      return "WRITE";
    }
    return "UNKNOWN";
}


/*
 * tokenFromString
 * ===============
 * 将规则文件中的 Token 名称映射为 TokenType
 * 用于 Lexer Generator
 */
inline TokenType tokenFromString(const std::string& s) {
    // ===== 控制 / 结束 =====
    if (s == "ENDFILE") return TokenType::ENDFILE;
    if (s == "ERROR")   return TokenType::ERROR;

    // ===== 标识符 & 常量 =====
    if (s == "ID")  return TokenType::ID;
    if (s == "NUM") return TokenType::NUM;

    // ===== 关键字 =====
    if (s == "INT")    return TokenType::INT;
    if (s == "VOID")   return TokenType::VOID;
    if (s == "IF")     return TokenType::IF;
    if (s == "ELSE")   return TokenType::ELSE;
    if (s == "WHILE")  return TokenType::WHILE;
    if (s == "RETURN") return TokenType::RETURN;

    // ===== 运算符 =====
    if (s == "ASSIGN") return TokenType::ASSIGN;
    if (s == "PLUS")   return TokenType::PLUS;
    if (s == "MINUS")  return TokenType::MINUS;
    if (s == "MULT")   return TokenType::MULT;
    if (s == "DIV")    return TokenType::DIV;

    // ===== 关系运算符 =====
    if (s == "LT")  return TokenType::LT;
    if (s == "GT")  return TokenType::GT;
    if (s == "LTE") return TokenType::LTE;
    if (s == "GTE") return TokenType::GTE;
    if (s == "EQ")  return TokenType::EQ;
    if (s == "NEQ") return TokenType::NEQ;

    // ===== 界符 =====
    if (s == "LPAREN") return TokenType::LPAREN;
    if (s == "RPAREN") return TokenType::RPAREN;
    if (s == "LBRACE") return TokenType::LBRACE;
    if (s == "RBRACE") return TokenType::RBRACE;
    if (s == "SEMI")   return TokenType::SEMI;
    if (s == "COMMA")  return TokenType::COMMA;

    if (s == "READ")  return TokenType::READ;
    if (s == "WRITE") return TokenType::WRITE;


    throw std::runtime_error("Unknown token name: " + s);
}
