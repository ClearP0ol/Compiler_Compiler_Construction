#pragma once
#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>

enum class TokenType
{
	// 控制与结束
	ENDFILE, // 输入结束标志
	ERROR,	 // 词法错误

	READ,
	WRITE,

	// 标识符与字面量
	ID,	 // 变量名/函数名
	NUM, // 整型常量

	// 控制流关键字
	IF,		// 条件
	ELSE,	// 分支
	WHILE,	// 循环
	RETURN, // 返回

	// 类型关键字
	INT,  // 整型
	VOID, // 无返回值

	// 算术运算符
	PLUS,  // +
	MINUS, // -
	MULT,  // *
	DIV,   // /

	// 关系/比较运算符
	GT,	 // >
	LT,	 // <
	GTE, // >=
	LTE, // <=
	EQ,	 // ==
	NEQ, // !=

	// 赋值运算符
	ASSIGN, // =

	// 界符与结构符号
	LPAREN, // (
	RPAREN, // )
	LBRACE, // {
	RBRACE, // }
	SEMI,	// ;
	COMMA	// ,
};

#endif // TOKEN_HPP