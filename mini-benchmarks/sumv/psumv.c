# include <stdio.h>
# include <unistd.h>
# include <math.h>
# include <float.h>
# include <limits.h>
# include <sys/time.h>
# include <stdlib.h>
#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif
#ifndef STREAM_ARRAY_SIZE
#   define STREAM_ARRAY_SIZE	10000000
#endif
#ifndef OFFSET
#   define OFFSET	0
#endif
#ifdef NTIMES
#if NTIMES<=1
#   define NTIMES	1
#endif
#endif
#ifndef NTIMES
#   define NTIMES	10
#endif
static STREAM_TYPE      sum_a,sum_b,sum_c;
int
main(){
	int i,k;
	STREAM_TYPE *a,*b,*c;
        
	a = (STREAM_TYPE *)malloc( ( STREAM_ARRAY_SIZE + OFFSET ) * sizeof(STREAM_TYPE));
        b = (STREAM_TYPE *)malloc( ( STREAM_ARRAY_SIZE + OFFSET ) * sizeof(STREAM_TYPE));
        c = (STREAM_TYPE *)malloc( ( STREAM_ARRAY_SIZE + OFFSET ) * sizeof(STREAM_TYPE));
        
	if (!a || !b || !c) {
                printf("cannot allocate valid memory\n");
                exit(0);
        }
	

	#pragma omp parallel for
	for (i=0; i<STREAM_ARRAY_SIZE+OFFSET; i++){ 
		a[i] = 0.0;
		b[i] = 1.0;
		c[i] = 2.0;
	}
	for (k=0; k<NTIMES; k++){
	#pragma omp parallel for
			for (i=0; i<STREAM_ARRAY_SIZE+OFFSET; i++){
				sum_a+=a[i];
				sum_b+=b[i];
				sum_c+=c[i];
			}
	}

	free(a);
	free(b);
	free(c);

}
