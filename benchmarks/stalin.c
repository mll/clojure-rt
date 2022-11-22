#include "stdio.h"
#include <math.h>

double improve(double guess, double x);
double average(double x, double y);

int good_enough(double guess, double x){
  if (fabs(guess*guess-x)<.001)
    return 1;
  return 0;
}


double sqrt_iter(double guess, double x){
  if( good_enough(guess, x))
    return guess;
  else
    return sqrt_iter( improve(guess,x), x);
}

double improve(double guess, double x){
  return average(guess, x/guess);
}

double average(double x, double y){
  return (x+y)/2;
}

double mysqrt(double x){
  return sqrt_iter(1.0, x);
}

int main(){
  double rez = 0;
  double x;
  double step = .001;
  for(x=0; x<= 10000; x+=step)
    rez += mysqrt(x)*step;
  printf("%f\n", rez);
  return 0;
}