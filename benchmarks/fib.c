#include <stdio.h>
#include <stdlib.h>

int64_t fib(int64_t x) {
    if (x==1) return 1;
    if (x==2) return 1;
    return fib(x-1) + fib(x-2);
}

int main() {
    printf("%lld", fib(42));
    return 0;
}
