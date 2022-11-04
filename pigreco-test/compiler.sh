#!/bin/bash
mpicc -Wall -fPIC pigreco.c -o main -I$PAPI_INC -I$PAPI_HOME/share/papi/testlib -L$PAPI_LIB -L$PAPI_HOME/share/papi/testlib -ltestlib -lpapi