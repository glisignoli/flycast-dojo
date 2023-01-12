FROM ubuntu:20.04

LABEL maintainer="glisignoli"

RUN sed -i 's/archive\.ubuntu\.com/nz.archive.ubuntu.com/g' /etc/apt/sources.list

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get install -y build-essential \
	libasound2 \
	libegl1-mesa-dev \
	libgles2-mesa-dev \
	libasound2-dev \
	mesa-common-dev \
	libgl1-mesa-dev \
	libudev-dev \
	libpulse-dev \
	libasio-dev \
	libzip-dev \
	libxext-dev \
	libcurl4-gnutls-dev \
	git \
	cmake \
	man-db \
	vim \
	libsndfile1-dev \
	rsync \
	ninja-build

# Add group 1000
RUN groupadd -g 1000 user

# Add user 1000
RUN useradd -u 1000 -g 1000 -m user

RUN ln -s /flycast-dojo/build.sh /build.sh
