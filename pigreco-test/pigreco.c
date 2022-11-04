#include <papi.h>
#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "papi_test.h"

int main(argc,argv)
int argc;
char *argv[];
{
  int  n, myid, numprocs, i, retval, EventSet = PAPI_NULL, code=-1;
  double PI25DT = 3.141592653589793238462643;
  double mypi, pi, h, sum, x;
  //char eventname[] ="rapl:::PP0_ENERGY_CNT:PACKAGE0";
  //char eventname[] ="PP0_ENERGY_CNT:PACKAGE0";
  //char eventname[] ="MSR_INTEL_PP0_ENERGY_STATUS";
  char eventname[] ="PP0_ENERGY:PACKAGE0";
  long_long values[1] = {(long_long) 0};

  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&myid);

  /*Initialize the PAPI library */
  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT){
    test_fail(__FILE__, __LINE__, "PAPI_library_init failed\n", retval);
    perror("PAPI_library_init failed\n");
    //exit(1);
  }

  /* Create an EventSet */
  retval = PAPI_create_eventset(&EventSet);
  if (retval != PAPI_OK){
    test_fail(__FILE__, __LINE__, "PAPI_create_eventset failed\n", retval);
    perror("PAPI_create_eventset failed\n");
    //exit(1);
  }

  retval = PAPI_event_name_to_code(eventname, &code);
    if (retval != PAPI_OK){
        test_fail(__FILE__, __LINE__, "PAPI_event_name_to_code", retval); 
        perror("PAPI_event_name_to_code\n");
        //exit(1);
    }

  /* Add Total Instructions Executed to our EventSet */
  retval = PAPI_add_event(EventSet, code);
  if (retval != PAPI_OK){
    test_fail(__FILE__, __LINE__, "PAPI_add_eventset failed\n", retval);
    perror("PAPI_add_eventset failed\n");
    //exit(1);
  }

  /* Start counting */
  retval = PAPI_start(EventSet);
  if (retval != PAPI_OK){
    test_fail(__FILE__, __LINE__, "PAPI_start failed\n", retval);
    perror("PAPI_start failed\n");
    //exit(1);
  }

  
    n=1000;
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    h   = 1.0 / (double) n;
    sum = 0.0;
    for (i = myid + 1; i <= n; i += numprocs) {
        x = h * ((double)i - 0.5);
        sum += 4.0 / (1.0 + x*x);
    }
    mypi = h * sum;

    MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0,MPI_COMM_WORLD);

    if (myid == 0)
        printf("pi is approximately %.16f, Error is %.16f\n", pi, fabs(pi - PI25DT));
    

   /* Read the counters */
   retval = PAPI_read(EventSet, values);
   if (retval != PAPI_OK) {
    test_fail(__FILE__, __LINE__, "PAPI_read failed\n", retval);
    perror("PAPI_read failed\n");
    //exit(1);
   }
   printf("After reading counters: %lld\n",values[0]);

   /* Stop the counters */
   retval = PAPI_stop(EventSet, values);
   if (retval != PAPI_OK){
    test_fail(__FILE__, __LINE__, "PAPI_stop failed\n", retval);
    perror("PAPI_stop failed\n");
    //exit(1);
   }
   printf("After stopping counters: %lld\n",values[0]);

   MPI_Finalize();
}