FROM ubuntu:22.04
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get install -y cmake clang git
WORKDIR /usr/src
RUN git clone https://github.com/oracle/odpi.git
RUN git clone https://github.com/yalcantara/cpplib-core.git
WORKDIR /usr/src/cpplib-dpiw
RUN mkdir cmake-build-debug
CMD ["/bin/bash"]