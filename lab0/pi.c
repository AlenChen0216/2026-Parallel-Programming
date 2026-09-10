#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define LL long long
#define N 10000000

int main(){
    srand(time(NULL));
    LL number_in_circle = 0;
    for(LL i = 0; i < N; i++){
        double x = (double)rand() / RAND_MAX;
        double y = (double)rand() / RAND_MAX;
        if(x*x + y*y <= 1){
            number_in_circle++;
        }
    }
    double pi_estimate = (4.0 * number_in_circle) / N;
    printf("%.2f\n",pi_estimate);
    return 0;
}