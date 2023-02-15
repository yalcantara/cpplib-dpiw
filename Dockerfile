FROM ubuntu:23.04
RUN apt-get update
RUN apt-get --assume-yes install cmake clang git
WORKDIR /usr/src
RUN git clone https://github.com/oracle/odpi.git
RUN git clone https://github.com/yalcantara/cpplib-core.git
WORKDIR /usr/src/cpplib-dpiw
RUN mkdir cmake-build-debug
CMD ["/bin/bash"]