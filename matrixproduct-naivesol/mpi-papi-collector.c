#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <papi.h>
#include <pthread.h>

#ifndef NDEBUG
#define PAPI_CHECK(PAPI_CMD, MSG)                                              \
  do {                                                                         \
    int retval = (PAPI_CMD);                                                   \
    if ((retval) != PAPI_OK) {                                                 \
      PAPI_perror(MSG);                                                        \
    }                                                                          \
  } while (0);
#else /* NDEBUG */
#define PAPI_CHECK(PAPI_CMD, MSG) (PAPI_CMD);
#endif /* NDEBUG */

static int eventSet = PAPI_NULL;

int MPI_Init_M(int *argc, char ***argv) {
  int ret = MPI_SUCCESS;

  char *eventnames_env = getenv("MPI_PAPI_COLLECTOR_EVENTS");
  char *eventnames = (char *)malloc(strlen(eventnames_env) + 1);
  strcpy(eventnames, eventnames_env);

  if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
    PAPI_perror("Error initializing PAPI library");
  }
  PAPI_CHECK(PAPI_thread_init(pthread_self) != PAPI_OK,
             "Error initializing PAPI thread support");

  /* adding events */
  PAPI_CHECK(PAPI_create_eventset(&eventSet), "Error to create PAPI event set");

  char *saveptr = NULL;
  char *eventName = strtok_r(eventnames, ",", &saveptr);
  while (eventName != NULL) {
    PAPI_CHECK(PAPI_add_named_event(eventSet, eventName),
               "Error adding PAPI event");
    eventName = strtok_r(NULL, ",", &saveptr);
  }
  free(eventnames);

  PAPI_CHECK(PAPI_start(eventSet), "Failed starting PAPI counters.");

  ret = PMPI_Init(argc, argv);
  return ret;
}

int MPI_Finalize_M() {
  int ret = MPI_SUCCESS;

  int num_events = PAPI_num_events(eventSet);
  if (num_events <= 0) {
    PAPI_perror("Failed determining the number of PAPI counters.");
  }
  long long *counters = (long long *)malloc(num_events * sizeof(long long));

  PAPI_stop(eventSet, counters);

  int world_rank = 0;
  int world_size = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // MPI_Allreduce(MPI_IN_PLACE, &counters, num_events, MPI_LONG_LONG,
  // MPI_SUM, MPI_COMM_WORLD);
  long long *all_counters = NULL;
  int root = 0;
  if (world_rank == root) {
    all_counters =
        (long long *)malloc(world_size * num_events * sizeof(long long));
  }
  MPI_Gather(counters, num_events, MPI_LONG_LONG, all_counters, num_events,
             MPI_LONG_LONG, root, MPI_COMM_WORLD);
  free(counters);

  if (world_rank == 0) {
    FILE *fh = stdout;

    int events[num_events];
    PAPI_list_events(eventSet, events, &num_events);
    fprintf(
        fh,
        "\n\n---------- COUNTER SUMMARY accross MPI processes ----------\n");
    fprintf(fh, "%s\t", "rank");
    for (int i = 0; i < num_events; i++) {
      char eventName[PAPI_MAX_STR_LEN];
      PAPI_event_code_to_name(events[i], eventName);
      fprintf(fh, "%s\t", eventName);
    }
    fprintf(fh, "\n");

    for (int rank = 0; rank < world_size; rank++) {
      fprintf(fh, "%d\t", rank);
      for (int i = 0; i < num_events; i++) {
        int idx = rank * num_events + i;
        fprintf(fh, "%lld\t", all_counters[idx]);
      }
      fprintf(fh, "\n");
    }
    free(all_counters);
  }

  ret = PMPI_Finalize();
  return ret;
}