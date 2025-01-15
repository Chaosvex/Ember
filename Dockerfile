# syntax=docker/dockerfile:experimental

FROM ubuntu:noble AS builder
LABEL description="Development build environment"

# Update the distro and install our tools
RUN apt-get -y update && apt-get -y upgrade \
 && apt-get -y install software-properties-common \
 && apt-get -y install wget \
 # GCC stuff
 && apt-get -y install build-essential gcc-14 g++-14 \
 # Clang stuff
 #&& wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null \
 #&& add-apt-repository -y 'deb http://apt.llvm.org/noble/ llvm-toolchain-noble-18 main' \
 #&& apt update \
 #&& apt-get -y install clang-18 \
 && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-14 100 \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 100 \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 \
 && apt-get -y install libstdc++-14-dev \
 && apt-get -y install cmake \
 && apt-get -y install git \
 # Install required library packages
 && apt-get install -y libbotan-2-dev \
 && apt-get install -y libmysqlcppconn-dev \
 && apt-get install -y zlib1g-dev \
 && apt-get install -y libpcre3-dev \
 && apt-get install -y libflatbuffers-dev \
 && wget -q https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz \
 && tar -zxf boost_1_84_0.tar.gz \
 && cd boost_1_84_0 \
 && ./bootstrap.sh --with-libraries=system,program_options,headers \
 && ./b2 link=static install -d0 -j 2 cxxflags="-std=c++23"

# Copy source
ARG working_dir=/usr/src/ember
COPY . ${working_dir}
WORKDIR ${working_dir}

# CMake arguments
# These can be overriden by passing them through to `docker build`
ARG build_optional_tools=1
ARG pcre_static_lib=1
ARG disable_threads=0
ARG build_type=Rel
ARG install_dir=/usr/local/bin

# Generate Makefile & compile
RUN --mount=type=cache,target=build \
    cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=${build_type} \
    -DCMAKE_INSTALL_PREFIX=${install_dir} \
    -DBUILD_OPT_TOOLS=${build_optional_tools} \
    -DPCRE_STATIC_LIB=${pcre_static_lib} \
    -DDISABLE_EMBER_THREADS=${disable_threads} \
    -DBOTAN_ROOT_DIR=/usr/include/botan-2/ \
    -DBOTAN_LIBRARY=/usr/lib/x86_64-linux-gnu/libbotan-2.so \
    && cd build && make -j$(nproc) install \
    && cp -r ${working_dir}/tests/ . \
    && make test

FROM ubuntu:noble AS run_environment
ARG install_dir=/usr/local/bin
ARG working_dir=/usr/src/ember
WORKDIR ${install_dir}
RUN apt-get -y update \
 && apt-get install -y libbotan-2-19 \
 && apt-get install -y libmysqlcppconn7v5 \
 && apt-get install -y mysql-client
COPY --from=builder ${install_dir} ${install_dir}
RUN cp configs/*.dist .
COPY ./sql ${install_dir}/sql
COPY ./scripts ${install_dir}
COPY ./dbcs ${install_dir}/dbcs