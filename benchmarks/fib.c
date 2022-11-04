#include <stdio.h>
#include <stdlib.h>

int64_t fib(int64_t x) {
    if (x==1) return 1;
    if (x==2) return 1;
    return fib(x-1) + fib(x-2);
}

int aaa(double a) {
    if(a == 3.0) return 1;
    return 2;
}

int factorial (int x, int y){
  if (x==0)
    return y;
  else
    return factorial(x-1,y*x);
}


void *zomo;

int main() {
    zomo = "aa";
    printf("%s", zomo);
    return fib(40);
}