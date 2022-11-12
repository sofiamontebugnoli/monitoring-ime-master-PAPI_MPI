#ifndef _papi__rapl_monitoring_h
#define _papi_rapl_monitoring_h

long long start_monitoring(int argc, char **argv, long long *values, int *EventSet);

void end_monitoring(long long * values, int *EventSet, long long start_time, char processor_name[MPI_MAX_PROCESSOR_NAME+1], MPI_Comm *MPI_COMM_NODE );

#endif