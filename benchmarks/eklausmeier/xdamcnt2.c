/*
       n Damen Problem loesen;
       DAMCOUNT;
       entworfen am 31.03.1985;
       geschrieben am 02.04.1985;
       revidiert am 18.04.1985 Do vorm.

       in C++ umgewandelt am 17.04.1994

     2   4    6     8      10         12           14
   1 0 0 2 10 4 40 92 352 724 2680 14200 73712 365596
*/


#include <stdio.h>
#include <stdlib.h>


#define abs(x)   ((x >= 0) ? x : -x)

/* Check if k-th queen is attacked by any other prior queen.
   Return nonzero if configuration is OK, zero otherwise.
*/
//inline	--> not faster, klm, 24-Apr-2020
int configOkay (int k, int a[]) {
    int z = a[k];

    for (int j=1; j<k; ++j) {
	int l = z - a[j];
	if (l == 0  ||  abs(l) == k - j) return 0;
    }
    return 1;
}



long solve (int N, int a[]) {  // return number of positions
    long cnt = 0;
    int k = a[1] = 1;
    int N2 = N;  //(N + 1) / 2;

    loop:
	if (configOkay(k,a)) {
	    if (k < N)  { a[++k] = 1;  goto loop; }
	    else ++cnt;
	}
	do
	    if (a[k] < N)  { a[k] += 1;  goto loop; }
	while (--k > 1);
	a[1] += 1;
	if (a[1] > N2) return cnt;
	k = 2 ,  a[2] = 1;
    goto loop;
}



int main (int argc, char *argv[]) {
    enum {NMAX=100};
    int a[NMAX];

    puts(" n-queens problem.\n"
    "   2   4    6     8      10         12           14\n"
    " 1 0 0 2 10 4 40 92 352 724 2680 14200 73712 365596\n"
    );

    int start = 1;
    int end = 0;
    if (argc > 1)  end = atoi(argv[1]);
    if (end <= 0  ||  end >= NMAX)  end = 10;
    if (argc > 2) { start = end;  end = atoi(argv[2]); }
    if (end <= 0  ||  end >= NMAX)   end = 10;

    for (int n=start; n<=end; ++n)
	printf (" D(%2d) = %ld\n", n, solve(n,a));

    return 0;
}