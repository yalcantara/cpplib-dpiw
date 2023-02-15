#!/bin/bash
CPD=/usr/src/cpplib-dpiw
docker run -it \
    --rm \
    -v ${PWD}/include:${CPD}/include \
    -v ${PWD}/CMAkeLists.txt:${CPD}/CMakeLists.txt \
    -v ${PWD}/Docker_Debug:${CPD}/Debug \
    -v ${PWD}/main.cpp:${CPD}/main.cpp \
    -w ${CPD}/Debug \
    cpplib-dpiw bash -c "
       cmake -DCMAKE_BUILD_TYPE=Debug ../ && \
       make
    "