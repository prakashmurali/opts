#include "main.h"

expr ite_gen(expr &a, expr &b, expr &c){
  return (3+ite(a, b, c));
}
void test(){
    std::cout << "if-then-else example2\n";
    context c;
    expr b = c.bool_const("b");
    expr x = c.int_const("x");
    expr y = c.int_const("y");
    std::cout << (ite(b, x, y) > 0) << "\n";
    expr d = ite_gen(b, x, y);
    cout << d << "\n";

    expr x1 = c.int_const("px");
    expr x2 = c.int_const("py");
    expr minx = c.int_const("MinX");
    expr maxx = c.int_const("MaxX");
  
    expr tmp = MIN_MAX_COND(x1, x2, minx, maxx);
    cout << tmp << "\n";

    expr lx = c.int_const("lx");
    expr ly = c.int_const("ly");
    expr rx = c.int_const("rx");
    expr ry = c.int_const("ry");

    expr tmp2 = POINT_IN_RECT(lx-3, ly-1, rx+3, ry*4, x1, x2);
    cout <<  tmp2 << "\n";

}
