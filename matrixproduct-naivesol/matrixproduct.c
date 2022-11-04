#define DIM 16
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <mpi.h>
#include "mpi-papi-collector.h"


int main(int argc, char *argv[])
{
    MPI_Init_M(&argc, &argv);
     
    int size, rank;
    double begin,end, local_elaps, global_elaps;// tempi
    int  d;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //verifiche su n e dim:
    if(size > DIM){
        if (rank==0) printf("il numero di processi %d è maggiore della dimensione della matrice %d.\n",size, DIM);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
   if(DIM%size!=0){
        if (rank==0) printf("il numero di processori non va bene \n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
// istante inizio:
    MPI_Barrier(MPI_COMM_WORLD);
    begin=MPI_Wtime();
    

    int A[DIM][DIM],B[DIM][DIM],C[DIM][DIM], i,j,k;
    int my_A[DIM*DIM], my_C[DIM][DIM];
    
     
    if (rank == 0){ // master:
        // INIZIALIZZAZIONE A E B:
        for(i=0;i<DIM;i++){
            for(j=0;j<DIM;j++){
                A[i][j]=i;
                B[i][j]=j;
            }
        }
        
        printf("[processo %d] matrice A:\n", rank);
        for(i=0;i<DIM;i++){
            for(j=0;j<DIM;j++)
                printf("%d\t",A[i][j]);
            printf("\n");
        }
        // invio i dati ad ogni processo
        
        //invio ad ogni processo Pk d=DIM/size righe
        d=DIM/size; // quante righe di A inviare ad ogni processo
        for (k=1; k<size; k++)
        {   //indice prima riga: k*d; indice prima colonna: 0
            i=k*d; 
            printf("RANK 0: invio %d righe al processo %d: indice prima riga:%d\n\n", d,k,i);
            MPI_Send(&A[i][0], d*DIM, MPI_INT, k,0, MPI_COMM_WORLD);//invio d righe di A
           
        }
        for (i=0; i<d; i++)
            for(j=0; j<DIM; j++)
                my_A[DIM*i+j]=A[i][j];
        
    }
    else {
        d=DIM/size;
        MPI_Recv(&my_A,d*DIM, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
    }
    
    //broadcast matrix B a tutti
    MPI_Bcast(&B, DIM*DIM, MPI_INT, 0, MPI_COMM_WORLD);
    
    
    // a questo punto ogni processo ha my_A e B --> calcolo
    for(i=0;i<DIM/size;i++) {
        for(j=0;j<DIM;j++){
            my_C[i][j]=0;
            for( k=0;k<DIM;k++){
                my_C[i][j]+=my_A[DIM*i+k]*B[k][j];
            }
        }
    }
    
     
    if (rank==0) { 
        for (i=0; i<d; i++) // copio i risultati di rank 0 in C
            for(j=0; j<DIM; j++)
                C[i][j]=my_C[i][j];
                
       //ricevo da ogni processo Pk d=DIM/size righe di C
        
        for (k=1; k<size; k++)
        {   //indice prima riga: k*d
            i=k*d; 
            MPI_Recv(&C[i], d*DIM, MPI_INT, MPI_ANY_SOURCE,0, MPI_COMM_WORLD,MPI_STATUS_IGNORE );
           
        }
    }
    else // slave
        MPI_Send(&my_C[0][0], d*DIM, MPI_INT, 0,0, MPI_COMM_WORLD);
    
    //misura tempo:
    MPI_Barrier(MPI_COMM_WORLD);
    end=MPI_Wtime();
    local_elaps= end-begin;
    printf("tempo impiegato da proc %d: %f secondi\n", rank, local_elaps);
    MPI_Reduce(&local_elaps, &global_elaps,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
	if (rank == 0) 
	{   	printf("Con n = %d processi e dimensione %d il tempo impiegato è: %f secondi\n",size, DIM, global_elaps);
	}
	

    if (rank == 0){
        printf("[processo %d] matrice C:\n", rank);
        for(i=0;i<DIM;i++){
            for(j=0;j<DIM;j++)
                printf("%d\t",C[i][j]);
            printf("\n");
        }
    }
    
    MPI_Finalize_M();
    
}