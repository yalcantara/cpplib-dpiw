#!/bin/bash
#CPD: Container Project Dir
CPD=/usr/src/cpplib-dpiw
docker run -it \
    --rm \
    -v ${PWD}/Docker_Debug:${CPD}/Debug \
    -w ${CPD}/Debug \
     cpplib-dpiw bash -c "
        echo \"Docker container created. About to run program main.\" && \
        ./bin/main
     "