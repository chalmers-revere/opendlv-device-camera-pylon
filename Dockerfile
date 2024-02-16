# Copyright (C) 2020  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Part to build opendlv-device-camera-pylon.
FROM ubuntu:20.04 as builder
MAINTAINER Christian Berger "christian.berger@gu.se"

# Set the env variable DEBIAN_FRONTEND to noninteractive
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        build-essential \
        git \
        libx11-dev \
        wget && \
    apt-get clean
RUN cd /tmp && \
    git clone --depth 1 https://chromium.googlesource.com/libyuv/libyuv && \
    cd libyuv &&\
    make -f linux.mk libyuv.a && cp libyuv.a /usr/lib && cd include && cp -r * /usr/include
RUN cd /tmp && \
    wget https://www2.baslerweb.com/media/downloads/software/pylon_software/pylon_6.1.1.19861_x86_64_setup.tar.gz && \
    tar xvzf pylon_6.1.1.19861_x86_64_setup.tar.gz && \
    mkdir -p /opt/pylon6 && \
    tar xvzf pylon_6.1.1.19861_x86_64.tar.gz -C /opt/pylon6
ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install


# Part to deploy opendlv-device-camera-pylon.
FROM ubuntu:20.04
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y && \
    apt-get install -y --no-install-recommends \
        libx11-dev && \
    apt-get clean

WORKDIR /opt/pylon6/lib
COPY --from=builder /opt/pylon6/lib .

WORKDIR /usr/bin
COPY --from=builder /tmp/bin/opendlv-device-camera-pylon .

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/pylon6/lib

ENTRYPOINT ["/usr/bin/opendlv-device-camera-pylon"]

