FROM ubuntu:23.04
RUN apt-get update
RUN apt-get --assume-yes install cmake clang wget
WORKDIR /usr/src/oracle/odpi/include
RUN wget https://raw.githubusercontent.com/oracle/odpi/main/include/dpi.h
WORKDIR /usr/src/cpplib-dpiw
CMD ["/bin/bash"]