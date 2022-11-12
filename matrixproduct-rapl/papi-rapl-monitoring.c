#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mpi.h"
#include "papi.h"
#include "papi_test.h"

#define MAX_RAPL_EVENTS 64


long long PAPI_start_AND_time(int *EventSet){

    long long before_time = PAPI_get_real_nsec();
    int retval;
    retval = PAPI_start(*EventSet);
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_start()", retval);
    return before_time;
}

long long PAPI_stop_AND_time(int *EventSet, long long *values){

    long long after_time = PAPI_get_real_nsec();
    int retval;
    retval = PAPI_stop(*EventSet, values);
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_stop()", retval);
    return after_time;
}


void file_management(char filenames[num_events][MPI_MAX_PROCESSOR_NAME+PAPI_MIN_STR_LEN+6], 
                    char processor_name[MPI_MAX_PROCESSOR_NAME+1], MPI_File *fff[num_events], MPI_Comm *MPI_COMM_NODE, int num_events, event_names[num]){
    
    int access_mode = MPI_MODE_CREATE /* Create the file if it does not exist */
                    | MPI_MODE_EXCL /* The file must not exist, to avoid mistakenly erasing a file */
                    | MPI_MODE_WRONLY /* With write access */
                    | MPI_MODE_UNIQUE_OPEN ;/* The file will not be opened concurrently elsewhere */
    /* Open output files */
    for (int i = 0; i < num_events; i++){
        // only file for monitoring pp0, package and dram energy consumption
        sprintf(filenames[i], "%s_res.%s", processor_name, event_names[i]);
        
        if(MPI_File_open(*MPI_COMM_NODE, filenames[i], access_mode, MPI_INFO_NULL, fff[i]) != MPI_SUCCESS){
            printf("[MPI process] Failure in opening files.\n");
            MPI_Abort(*MPI_COMM_NODE, EXIT_FAILURE);
        }
        
    }
}


void PAPI_term(int *EventSet, long long *values)
{
    int retval;
    retval = PAPI_cleanup_eventset(*EventSet);
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_cleanup_eventset()", retval);

    retval = PAPI_destroy_eventset(&(*EventSet));
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_destroy_eventset()", retval);
}



long long *RAPL_plot_init(int *EventSet, char event_names[MAX_EVENTS][PAPI_MAX_STR_LEN], char units[MAX_RAPL_EVENTS][PAPI_MIN_STR_LEN], 
                            int *num_events, char units[MAX_RAPL_EVENTS][PAPI_MIN_STR_LEN], int data_type[MAX_EVENTS] )
{
    int retval, cid, numcmp, rapl_cid = -1, code = -1;
    const PAPI_component_info_t *cmpinfo = NULL;
    PAPI_event_info_t evinfo;
    int r;

    /* PAPI Initialization: viene verificata la versione */
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) test_fail(__FILE__, __LINE__, "PAPI_library_init failed\n", retval);

    /* Ricerca del componente RAPL */
    /* Scorre tra tutti i componenti di PAPI per trovare RAPL */
    numcmp = PAPI_num_components();
    for (cid = 0; cid < numcmp; cid++){
        
        if ((cmpinfo = PAPI_get_component_info(cid)) == NULL) test_fail(__FILE__, __LINE__, "PAPI_get_component_info failed\n", 0);

        if (strstr(cmpinfo->name, "rapl")){
            
            rapl_cid = cid;

            if (cmpinfo->disabled) test_skip(__FILE__, __LINE__, "RAPL component disabled", 0);
        
            break;
        }
    }
    /* Component RAPL not found */
    if (cid == numcmp) test_skip(__FILE__, __LINE__, "No rapl component found\n", 0);
    

    /* Create EventSet */
    retval = PAPI_create_eventset(&(*EventSet));
    if (retval != PAPI_OK)
    {
        test_fail(__FILE__, __LINE__, "PAPI_create_eventset()", retval);
    }

    /* Aggiunge a EventSet all RAPL events disponibili */
    code = PAPI_NATIVE_MASK; // = 0x40000000
    /* (in riferimento al componente rapl identificato da rapl_cid)
        sostituisce a code il primo evento disponibile
    */
    r = PAPI_enum_cmp_event(&code, PAPI_ENUM_FIRST, rapl_cid);
    while (r == PAPI_OK)
    {

        retval = PAPI_event_code_to_name(code, event_names[*num_events]);
        if (retval != PAPI_OK) {
            printf("Error translating %#x\n", code);
            test_fail(__FILE__, __LINE__, "PAPI_event_code_to_name\n", retval);
        }

        retval = PAPI_get_event_info(code, &evinfo);
        if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "Error getting event info\n", retval);
        
        strncpy(units[*num_events], evinfo.units, sizeof(units[0]));
        // buffer must be null terminated to safely use strstr operation on it below
        units[*num_events][sizeof(units[0]) - 1] = '\0';

        data_type[*num_events] = evinfo.data_type;

        retval = PAPI_add_event((*EventSet), code);

        if (retval != PAPI_OK)
            break; /* We've hit an event limit */
        
        (*num_events)++;

        // sostituisce a code il prossimo evento disponibile
        r = PAPI_enum_cmp_event(&code, PAPI_ENUM_EVENTS, rapl_cid);
    }

    /* Prepara il vettore values che conterrà i risultati*/
    long long *values = calloc(*num_events, sizeof(long long));
    
    if (values == NULL) test_fail(__FILE__, __LINE__, "No memory", retval);
    
    return values;
}


void RAPL_stop_AND_print(long long before_time, int *EventSet, long long *values, int num_events, 
char units[MAX_RAPL_EVENTS][PAPI_MIN_STR_LEN], char event_names[MAX_RAPL_EVENTS][PAPI_MAX_STR_LEN],
                         int data_type[MAX_RAPL_EVENTS])
{

  int retval, i;
  long long after_time;
  double elapsed_time;

 
  /* Varie print */
  if (!TESTS_QUIET)
  {
    printf("\nStopping measurements, took %.3fs, gathering results...\n\n", elapsed_time);

    printf("Scaled energy measurements:\n");

    for (i = 0; i < num_events; i++)
    {
      if (strstr(units[i], "nJ"))
      {

        printf("%-40s%12.6f J\t(Average Power %.1fW)\n",
               event_names[i],
               (double)values[i] / 1.0e9,
               ((double)values[i] / 1.0e9) / elapsed_time);
      }
    }

    printf("\n");
    printf("Energy measurement counts:\n");

    for (i = 0; i < num_events; i++)
    {
      if (strstr(event_names[i], "ENERGY_CNT"))
      {
        printf("%-40s%12lld\t%#08llx\n", event_names[i], values[i], values[i]);
      }
    }

    printf("\n");
    printf("Scaled Fixed values:\n");

    for (i = 0; i < num_events; i++)
    {
      if (!strstr(event_names[i], "ENERGY"))
      {
        if (data_type[i] == PAPI_DATATYPE_FP64)
        {

          union
          {
            long long ll;
            double fp;
          } result;

          result.ll = values[i];
          printf("%-40s%12.3f %s\n", event_names[i], result.fp, units[i]);
        }
      }
    }

    printf("\n");
    printf("Fixed value counts:\n");

    for (i = 0; i < num_events; i++)
    {
      if (!strstr(event_names[i], "ENERGY"))
      {
        if (data_type[i] == PAPI_DATATYPE_UINT64)
        {
          printf("%-40s%12lld\t%#08llx\n", event_names[i], values[i], values[i]);
        }
      }
    }
  }

}



void end_monitoring(int *EventSet, long long *values, int num_events, char units[MAX_RAPL_EVENTS][PAPI_MIN_STR_LEN], 
                    char event_names[MAX_RAPL_EVENTS][PAPI_MAX_STR_LEN], int data_type[MAX_RAPL_EVENTS], 
                    long long start_time, char processor_name[MPI_MAX_PROCESSOR_NAME+1], MPI_Comm *MPI_COMM_NODE ){
    char filenames[num_events][MPI_MAX_PROCESSOR_NAME+PAPI_MIN_STR_LEN+6];
    MPI_File *fff[num_events];
    long long stop_time ;
    double total_time;
    int max_len=1024;
    char buf[max_len];
    
    //papi stop = stop monitoring and time
    stop_time=PAPI_stop_AND_time(EventSet, values);
    total_time = ((double)(stop_time - start_time)) / 1.0e9;
    //open all file to save values of the processor in it
    file_management(filenames, processor_name, fff, MPI_COMM_NODE);
    
    snprintf(buf_PAPI_REF_CYC, max_len, "%.04f %.3f \n", total_time, ((double)values[PAPI_REF_CYC] / 1.0e6));
    snprintf(buf_PAPI_SP_OPS, max_len, "%.04f %.3f \n", total_time, ((double)values[PAPI_SP_OPS] / 1.0e6));
    snprintf(buf_PAPI_VEC_SP, max_len, "%.04f %.3f \n", total_time, ((double)values[PAPI_VEC_SP] / 1.0e6));
    MPI_File_write(*fff[PAPI_REF_CYC], buf_PAPI_REF_CYC, strlen(buf_PAPI_REF_CYC), MPI_LONG_LONG_INT, MPI_STATUS_IGNORE);
    MPI_File_write(*fff[PAPI_SP_OPS], buf_PAPI_SP_OPS, strlen(buf_PAPI_SP_OPS), MPI_LONG_LONG_INT, MPI_STATUS_IGNORE);
    MPI_File_write(*fff[PAPI_VEC_SP], buf_PAPI_VEC_SP, strlen(buf_PAPI_VEC_SP), MPI_LONG_LONG_INT, MPI_STATUS_IGNORE);


    snprintf(buf, max_len, "%s\n", "Scaled energy measurements:\n");
   

    for (i = 0; i < num_events; i++)
    {
      if (strstr(units[i], "nJ"))
      {

        printf("%-40s%12.6f J\t(Average Power %.1fW)\n",
               event_names[i],
               (double)values[i] / 1.0e9,
               ((double)values[i] / 1.0e9) / elapsed_time);
      }
    }



    for (int i = 0; i < MAX_EVENTS; i++){
        if (i == PAPI_REF_CYC || i == PAPI_SP_OPS || i == PAPI_VEC_SP){
            if(MPI_File_close(fff[i]) != MPI_SUCCESS){
                printf("[MPI process] Failure in closing files.\n");
                MPI_Abort(*MPI_COMM_NODE, EXIT_FAILURE);
            }
        }
    }

    
    PAPI_term(EventSet, values);
    //exit(EXIT_SUCCESS);

}