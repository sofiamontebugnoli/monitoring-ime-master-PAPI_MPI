#define PAPI_REF_CYC 0
#define PAPI_SP_OPS 10
#define PAPI_VEC_SP 17
#define MAX_EVENTS 16

#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stddef.h>
#include <unistd.h>
#include <mpi.h>
#include <time.h>
#include "papi.h"
#include "papi_test.h"
#include <pthread.h>
#include <string.h>


static char event_names[MAX_EVENTS][PAPI_MAX_STR_LEN] = {

    "rapl:::THERMAL_SPEC_CNT:PACKAGE0",
    "rapl:::MINIMUM_POWER_CNT:PACKAGE0",
    "rapl:::MAXIMUM_POWER_CNT:PACKAGE0",
    "rapl:::MAXIMUM_TIME_WINDOW_CNT:PACKAGE0",
    "rapl:::PACKAGE_ENERGY_CNT:PACKAGE0",
    "rapl:::DRAM_ENERGY_CNT:PACKAGE0",
    "rapl:::PSYS_ENERGY_CNT:PACKAGE0",
    "rapl:::PP0_ENERGY_CNT:PACKAGE0",
    "rapl:::THERMAL_SPEC:PACKAGE0",
    "rapl:::MINIMUM_POWER:PACKAGE0",
    "rapl:::MAXIMUM_POWER:PACKAGE0",
    "rapl:::MAXIMUM_TIME_WINDOW:PACKAGE0",
    "rapl:::PACKAGE_ENERGY:PACKAGE0",
    "rapl:::DRAM_ENERGY:PACKAGE0",
    "rapl:::PSYS_ENERGY:PACKAGE0",
    "rapl:::PP0_ENERGY:PACKAGE0" /*,
    "powercap:::ENERGY_UJ:ZONE0",
    "powercap:::MAX_ENERGY_RANGE_UJ:ZONE0",
    "powercap:::MAX_POWER_A_UW:ZONE0",
    "powercap:::POWER_LIMIT_A_UW:ZONE0",
    "powercap:::TIME_WINDOW_A_US:ZONE0",
    "powercap:::MAX_POWER_B_UW:ZONE0",
    "powercap:::POWER_LIMIT_B_UW:ZONE0",
    "powercap:::TIME_WINDOW_B:ZONE0",
    "powercap:::ENABLED:ZONE0",
    "powercap:::NAME:ZONE0",
    "powercap:::ENERGY_UJ:ZONE0_SUBZONE0",
    "powercap:::MAX_ENERGY_RANGE_UJ:ZONE0_SUBZONE0",
    "powercap:::MAX_POWER_A_UW:ZONE0_SUBZONE0",
    "powercap:::POWER_LIMIT_A_UW:ZONE0_SUBZONE0",
    "powercap:::TIME_WINDOW_A_US:ZONE0_SUBZONE0",
    "powercap:::ENABLED:ZONE0_SUBZONE0",
    "powercap:::NAME:ZONE0_SUBZONE0",
    "powercap:::ENERGY_UJ:ZONE0_SUBZONE1",
    "powercap:::MAX_ENERGY_RANGE_UJ:ZONE0_SUBZONE1",
    "powercap:::MAX_POWER_A_UW:ZONE0_SUBZONE1",
    "powercap:::POWER_LIMIT_A_UW:ZONE0_SUBZONE1",
    "powercap:::TIME_WINDOW_A_US:ZONE0_SUBZONE1",
    "powercap:::ENABLED:ZONE0_SUBZONE1",
    "powercap:::NAME:ZONE0_SUBZONE1"*/
};

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
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_start()", retval);
    return after_time;
}


long long *PWCAP_plot_init(int argc, char **argv, int *EventSet, char event_names[MAX_EVENTS][PAPI_MAX_STR_LEN], char units[MAX_EVENTS][PAPI_MIN_STR_LEN], 
                            int data_type[MAX_EVENTS] )
{
    int retval, i;
    long long *values;
    int code = -1;
    PAPI_event_info_t evinfo;
    
    /* Set TESTS_QUIET variable */
    if(argc>=4){
        tests_quiet(argc, argv);
    }
    
    /* PAPI Initialization */
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) test_fail(__FILE__, __LINE__, "PAPI_library_init failed\n", retval);   

    /* initialization of the threads */
    retval= PAPI_thread_init(pthread_self);
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_library_init failed\n", retval);

    /* Create EventSet */
    retval = PAPI_create_eventset(&(*EventSet));
    if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "PAPI_create_eventset()", retval);

    for (i = 0; i < MAX_EVENTS; i++){
        retval = PAPI_event_name_to_code(event_names[i], &code);
        if (retval != PAPI_OK){
            printf("Error translating %s\n", event_names[i]);
            test_fail(__FILE__, __LINE__, "PAPI_event_name_to_code", retval); 
        }

        retval = PAPI_get_event_info(code, &evinfo);
        if (retval != PAPI_OK) test_fail(__FILE__, __LINE__, "Error getting event info\n", retval);

        strncpy(units[i], evinfo.units, sizeof(units[0]));
        // buffer must be null terminated to safely use strstr operation on it below
        units[i][sizeof(units[0]) - 1] = '\0';
        data_type[i] = evinfo.data_type;

        retval = PAPI_add_event(*EventSet, code);
        if (retval != PAPI_OK){
            test_fail(__FILE__, __LINE__, "hit an event limit\n", retval);
            break; /* We've hit an event limit */
        }
    }

    values = calloc(i, sizeof(long long));
    if (values == NULL) test_fail(__FILE__, __LINE__, "No memory", retval);

    return values;
}

void file_management(char filenames[MAX_EVENTS][MPI_MAX_PROCESSOR_NAME+PAPI_MIN_STR_LEN+6], 
                    char processor_name[MPI_MAX_PROCESSOR_NAME+1], MPI_File *fff[MAX_EVENTS], MPI_Comm *MPI_COMM_NODE){
    
    int access_mode = MPI_MODE_CREATE /* Create the file if it does not exist */
                    | MPI_MODE_EXCL /* The file must not exist, to avoid mistakenly erasing a file */
                    | MPI_MODE_WRONLY /* With write access */
                    | MPI_MODE_UNIQUE_OPEN ;/* The file will not be opened concurrently elsewhere */
    /* Open output files */
    for (int i = 0; i < MAX_EVENTS; i++){
        // only file for monitoring pp0, package and dram energy consumption
        sprintf(filenames[i], "%s_res.%s", processor_name, event_names[i]);
        if (i == PAPI_REF_CYC || i == PAPI_SP_OPS || i == PAPI_VEC_SP){
            if(MPI_File_open(*MPI_COMM_NODE, filenames[i], access_mode, MPI_INFO_NULL, fff[i]) != MPI_SUCCESS){
                printf("[MPI process] Failure in opening files.\n");
                MPI_Abort(*MPI_COMM_NODE, EXIT_FAILURE);
            }
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


long long start_monitoring(int argc, char **argv, long long *values, int *EventSet){
    
    long long start_time;
    char units[MAX_EVENTS][PAPI_MIN_STR_LEN];
    int data_type[MAX_EVENTS];
    //papi initialization
    values = PWCAP_plot_init(argc, argv, EventSet, event_names, units, data_type);
    //papi start = begin monitoring and time
    start_time=PAPI_start_AND_time(EventSet);
    return start_time;

}


void end_monitoring(long long * values, int *EventSet, long long start_time, char processor_name[MPI_MAX_PROCESSOR_NAME+1], MPI_Comm *MPI_COMM_NODE ){
    char filenames[MAX_EVENTS][MPI_MAX_PROCESSOR_NAME+PAPI_MIN_STR_LEN+6];
    MPI_File *fff[MAX_EVENTS];
    long long stop_time ;
    double total_time;
    int max_len=200;
    char buf_PAPI_REF_CYC[max_len];
    char buf_PAPI_SP_OPS[max_len];
    char buf_PAPI_VEC_SP[max_len];
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