类C语言：c_like.lex

tiny语言：tiny.lex

test_expr语言：expr.lex

对应的测试文件如下

类C语言：test_c_like.txt

tiny语言：test.txt

test_expr语言：test_expr.txt

运行多代码文件的命令如下：
```
mingw32-make
./lexer-gen
```
输出的token流文件：output.txt