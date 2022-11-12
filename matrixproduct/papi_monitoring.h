#ifndef _papi__rapl_monitoring_h
#define _papi_rapl_monitoring_h
#define ENERGY_UJ_ZONE0 0
#define ENERGY_UJ_ZONE0_SUBZONE0 10
//#define ENERGY_UJ_ZONE1 17
//#define ENERGY_UJ_ZONE1_SUBZONE0 27
#define MAX_EVENTS 17

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
"powercap:::ENERGY_UJ:ZONE0",         //0                                     
"powercap:::MAX_ENERGY_RANGE_UJ:ZONE0",                                      
 "powercap:::MAX_POWER_A_UW:ZONE0",                                              
 "powercap:::POWER_LIMIT_A_UW:ZONE0",                                           
 "powercap:::TIME_WINDOW_A_US:ZONE0",                                            
 "powercap:::MAX_POWER_B_UW:ZONE0",                                              
 "powercap:::POWER_LIMIT_B_UW:ZONE0",                                            
 "powercap:::TIME_WINDOW_B:ZONE0",                                             
 "powercap:::ENABLED:ZONE0",                                                   
 "powercap:::NAME:ZONE0",                                                       
 "powercap:::ENERGY_UJ:ZONE0_SUBZONE0",    //11                                     
 "powercap:::MAX_ENERGY_RANGE_UJ:ZONE0_SUBZONE0",                                
 "powercap:::MAX_POWER_A_UW:ZONE0_SUBZONE0",                                     
 "powercap:::POWER_LIMIT_A_UW:ZONE0_SUBZONE0",                                  
 "powercap:::TIME_WINDOW_A_US:ZONE0_SUBZONE0",                                  
 "powercap:::ENABLED:ZONE0_SUBZONE0",                                       
 "powercap:::NAME:ZONE0_SUBZONE0",                                             
 /*"powercap:::ENERGY_UJ_ZONE1",      //17                                         
 "powercap:::MAX_ENERGY_RANGE_UJ:ZONE1",                                        
 "powercap:::MAX_POWER_A_UW:ZONE1",                                            
 "powercap:::POWER_LIMIT_A_UW:ZONE1",                                            
 "powercap:::TIME_WINDOW_A_US:ZONE1",                                            
 "powercap:::MAX_POWER_B_UW:ZONE1",                                              
 "powercap:::POWER_LIMIT_B_UW:ZONE1",                                            
 "powercap:::TIME_WINDOW_B:ZONE1",                                            
 "powercap:::ENABLED:ZONE1",                                                     
 "powercap:::NAME:ZONE1",                                                       
 "powercap:::ENERGY_UJ_ZONE1_SUBZONE0",   //27                                      
 "powercap:::MAX_ENERGY_RANGE_UJ:ZONE1_SUBZONE0",                               
 "powercap:::MAX_POWER_A_UW:ZONE1_SUBZONE0",                                     
 "powercap:::POWER_LIMIT_A_UW:ZONE1_SUBZONE0",                                   
 "powercap:::TIME_WINDOW_A_US:ZONE1_SUBZONE0",                                  
 "powercap:::ENABLED:ZONE1_SUBZONE0",                                           
 "powercap:::NAME:ZONE1_SUBZONE0"*/
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
    char filenames[MPI_MAX_PROCESSOR_NAME+6];
    MPI_File fd;
    long long stop_time;
    double total_time;
    int max_len=256;
   
    char tot_time[max_len];
    
    stop_time=PAPI_stop_AND_time(EventSet, values);
    total_time = ((double)(stop_time - start_time)) / 1.0e9;
    

    //open all file to save values of the processor in it

    int access_mode = MPI_MODE_CREATE /* Create the file if it does not exist */
                    | MPI_MODE_EXCL /* The file must not exist, to avoid mistakenly erasing a file */
                    //| MPI_MODE_WRONLY /* With write access */
                    | MPI_MODE_RDWR 
                    | MPI_MODE_UNIQUE_OPEN;/* The file will not be opened concurrently elsewhere */
    /* Open output files */

    sprintf(filenames, "%s_res", processor_name);
    if(MPI_File_open(*MPI_COMM_NODE, filenames, access_mode, MPI_INFO_NULL, &fd) != MPI_SUCCESS){
        printf("[MPI process] Failure in opening files.\n");
        MPI_Abort(*MPI_COMM_NODE, EXIT_FAILURE);
    }

    snprintf(tot_time, max_len, "total time:\t%.04f\n", total_time);
    MPI_File_write(fd, tot_time, strlen(tot_time), MPI_LONG_LONG_INT, MPI_STATUS_IGNORE);
    for (int i = 0; i < MAX_EVENTS; i++){
        char buf[max_len];
        snprintf(buf, max_len, "%s:\t%.3f\n", event_names[i],  ((double)values[i] / 1.0e6));
        MPI_File_write(fd, buf, strlen(buf), MPI_LONG_LONG_INT, MPI_STATUS_IGNORE);
    }

    if(MPI_File_close(&fd) != MPI_SUCCESS){
        printf("[MPI process] Failure in closing files.\n");
        MPI_Abort(*MPI_COMM_NODE, EXIT_FAILURE);
    }

    PAPI_term(EventSet, values);
    //exit(EXIT_SUCCESS);

}
#endif