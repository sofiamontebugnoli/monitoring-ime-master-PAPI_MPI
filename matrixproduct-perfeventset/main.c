#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <stddef.h>
#include <unistd.h>
#include <mpi.h>
#include <time.h>
#include "papi.h"
#include <stdbool.h> 
#include "papi_test.h"
#include <pthread.h>
#include <string.h>
#include "papi_monitoring.h"

void  arg_check (int *n_nodes, int *n_ranks, int argc, char **argv){
    switch (argc)
    {
    case 3:
        /* first argument argv[1] = number of nodes arg[2]= total number of ranks */
        *n_nodes=atoi(argv[1]);
        *n_ranks=atoi(argv[2]);
        break;
    case 4:
        //argv[1] = test quiet -q or TEST_QUIET
        *n_nodes=atoi(argv[2]);
        *n_ranks=atoi(argv[3]);
        break;
    
    default:
        printf("USAGE: arg 1 should be the number of nodes required and arg 2 should be the total number of ranks");
        exit(-1);
        break;
    }

}

void resource_check(int rank, int size, int n_ranks){
    if(size > n_ranks){
        if (rank==0) printf("il numero di processi %d Ã¨ maggiore della n_ranksensione della matrice %d.\n",size, n_ranks);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
   if(n_ranks%size!=0){
        if (rank==0) printf("il numero di processori non va bene \n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
}

bool verify_rank(int rank, int size, int papiranks[], int papirankssize){
   bool verify=true;
    for( int i =0; i < papirankssize; i++){
        if (rank == papiranks[i]){
            verify = false; //tutti i rank di monitoraggio hanno verify a false 
        }
    }
    return verify;
}


int main(int argc, char **argv){

    //passing number of nodes and ranks as arguments
    int n_nodes=0;
    int n_ranks=0;
    //verify the arguments
    arg_check(&n_nodes, &n_ranks, argc, argv);

   // begin of MPI algoritm parrallel from now on
    MPI_Init(&argc, &argv);
     
    int size, rank;
    int  d;
    char processor_name [MPI_MAX_PROCESSOR_NAME+1];
    int len;
    int EventSet = PAPI_NULL;
    long long *values;
    long long start_time;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //verifiche su n e n_ranks:
    resource_check(rank, size, n_ranks);

    //division on groups based on nodes = shared type
    int sm_rank, sm_size;
    int mnt_rank=0;
    MPI_Comm MPI_COMM_NODE;
    if( MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &MPI_COMM_NODE) != MPI_SUCCESS){
        if (rank==0) printf("Impossible to perform the division");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        
    } //JUST IN CASE: change MPI_COMM_TYPE_SHARED into OMPI_COMM_TYPE_NODE which is speific for OPENMPI
     
    MPI_Comm_rank(MPI_COMM_NODE, &sm_rank);
    MPI_Comm_size(MPI_COMM_NODE, &sm_size);

    //determine ranks for monitoring 
    //if there is more than one rank per node the mnt_rank is always the last rank (numerically) in the node
    if(sm_size > 1){
        mnt_rank=sm_size-1;
    }

    // begin of the computation
    MPI_Barrier(MPI_COMM_WORLD);

    int A[n_ranks][n_ranks],B[n_ranks][n_ranks],C[n_ranks][n_ranks], i,j,k;
    int my_A[n_ranks*n_ranks], my_C[n_ranks][n_ranks];
    
    MPI_Barrier(MPI_COMM_NODE);
    //start monitoring 
    if(sm_rank==mnt_rank){
        //monitoring function papiStart
        printf("MPI process %d start monitoring", sm_rank);  
        start_time= start_monitoring(argc, argv, values, &EventSet);
    }

    //algoritm specific
     
     if (rank == 0){ // master:
        // INIZIALIZZAZIONE A E B:
        for(i=0;i<n_ranks;i++){
            for(j=0;j<n_ranks;j++){
                A[i][j]=i;
                B[i][j]=j;
            }
        }
        
        printf("[processo %d] matrice A:\n", rank);
        for(i=0;i<n_ranks;i++){
            for(j=0;j<n_ranks;j++)
                printf("%d\t",A[i][j]);
            printf("\n");
        }
        // invio i dati ad ogni processo
        
        //invio ad ogni processo Pk d=n_ranks/size righe
        d=n_ranks/size; // quante righe di A inviare ad ogni processo
        for (k=1; k<size; k++)
        {   //indice prima riga: k*d; indice prima colonna: 0
            i=k*d; 
            printf("RANK 0: invio %d righe al processo %d: indice prima riga:%d\n\n", d,k,i);
            MPI_Send(&A[i][0], d*n_ranks, MPI_INT, k,0, MPI_COMM_WORLD);//invio d righe di A
           
        }
        for (i=0; i<d; i++)
            for(j=0; j<n_ranks; j++)
                my_A[n_ranks*i+j]=A[i][j];
        
    }
    else {
        d=n_ranks/size;
        MPI_Recv(&my_A,d*n_ranks, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
    }
    
    //broadcast matrix B a tutti
    MPI_Bcast(&B, n_ranks*n_ranks, MPI_INT, 0, MPI_COMM_WORLD);
    
    
    // a questo punto ogni processo ha my_A e B --> calcolo
    for(i=0;i<n_ranks/size;i++) {
        for(j=0;j<n_ranks;j++){
            my_C[i][j]=0;
            for( k=0;k<n_ranks;k++){
                my_C[i][j]+=my_A[n_ranks*i+k]*B[k][j];
            }
        }
    }
    
     
    if (rank==0) { 
        for (i=0; i<d; i++) // copio i risultati di rank 0 in C
            for(j=0; j<n_ranks; j++)
                C[i][j]=my_C[i][j];
                
       //ricevo da ogni processo Pk d=n_ranks/size righe di C
        
        for (k=1; k<size; k++)
        {   //indice prima riga: k*d
            i=k*d; 
            MPI_Recv(&C[i], d*n_ranks, MPI_INT, MPI_ANY_SOURCE,0, MPI_COMM_WORLD,MPI_STATUS_IGNORE );
           
        }
        
    }
    else{ // slave
        MPI_Send(&my_C[0][0], d*n_ranks, MPI_INT, 0,0, MPI_COMM_WORLD);
    }
    
    MPI_Barrier(MPI_COMM_NODE);

    if(mnt_rank==sm_rank){
        //PAPI Stop monitoring
        printf("MPI process %d stop monitoring", sm_rank);  
        MPI_Get_processor_name(processor_name, &len);
        end_monitoring(values, &EventSet, start_time, processor_name, &MPI_COMM_NODE);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0){
        printf("[processo %d] matrice C:\n", rank);
        for(i=0;i<n_ranks;i++){
            for(j=0;j<n_ranks;j++)
                printf("%d\t",C[i][j]);
            printf("\n");
        }
    }
    
    MPI_Finalize();
    
    
   return 0;
}