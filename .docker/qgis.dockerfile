# syntax=docker/dockerfile:experimental

# see https://docs.docker.com/docker-cloud/builds/advanced/
# using ARG in FROM requires min v17.05.0-ce
ARG DOCKER_DEPS_TAG=latest

FROM --platform=$TARGETPLATFORM qgis/qgis3-build-deps:${DOCKER_DEPS_TAG} AS BUILDER
MAINTAINER Denis Rouzaud <denis@opengis.ch>

LABEL Description="Docker container with QGIS" Vendor="QGIS.org" Version="1.1"

# build timeout in seconds, so no timeout by default
ARG BUILD_TIMEOUT=360000

VOLUME /QGIS/.ccache_image_build

ARG CC=/usr/lib/ccache/gcc
ARG CXX=/usr/lib/ccache/g++
ENV LANG=C.UTF-8

# install mold
RUN apt-get update && apt-get install mold -y

COPY . /QGIS

# If this directory is changed, also adapt script.sh which copies the directory
# if ccache directory is not provided with the source
ENV CCACHE_DIR=/QGIS/.ccache_image_build
RUN ccache -M 16G
RUN ccache -s
ENV CCACHE_SLOPPINESS=clang_index_store,file_stat_matches,file_stat_matches_ctime,include_file_ctime,include_file_mtime,locale,pch_defines,modules,system_headers,time_macros
ENV CCACHE_MAXFILES=0
ENV CCACHE_NOHASHDIR=1

ENV CCACHE_DEBUG=1
ENV CCACHE_LOGFILE=/QGIS/.ccache_image_build/ccache.log

RUN echo "ccache_dir: "$(du -h --max-depth=0 ${CCACHE_DIR})

WORKDIR /QGIS/build
RUN SUCCESS=OK \
  && cmake \
  -GNinja \
  -DUSE_CCACHE=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DWITH_DESKTOP=ON \
  -DWITH_SERVER=OFF \
  -DWITH_3D=OFF \
  -DWITH_BINDINGS=ON \
  -DWITH_CUSTOM_WIDGETS=ON \
  -DBINDINGS_GLOBAL_INSTALL=ON \
  -DWITH_STAGED_PLUGINS=ON \
  -DWITH_GRASS=OFF \
  -DSUPPRESS_QT_WARNINGS=ON \
  -DDISABLE_DEPRECATED=ON \
  -DENABLE_TESTS=ON \
  -DWITH_QSPATIALITE=ON \
  -DWITH_APIDOC=OFF \
  -DWITH_ASTYLE=OFF \
  -DCMAKE_CXX_FLAGS_DEBUG="-g -O0" \
  -DCMAKE_C_FLAGS_DEBUG="-g -O0" \
  -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold" \
  -DCMAKE_MODULE_LINKER_FLAGS="-fuse-ld=mold" \
  ..

# Additional run-time dependencies
RUN pip3 install jinja2 pygments pexpect && apt install -y expect

################################################################################
# Python testing environment setup

# Add QGIS test runner
COPY .docker/qgis_resources/test_runner/qgis_* /usr/bin/

# Make all scripts executable
RUN chmod +x /usr/bin/qgis_*

# Add supervisor service configuration script
COPY .docker/qgis_resources/supervisor/ /etc/supervisor

# Python paths are for
# - kartoza images (compiled)
# - deb installed
# - built from git
# needed to find PyQt wrapper provided by QGIS
ENV PYTHONPATH=/usr/share/qgis/python/:/usr/share/qgis/python/plugins:/usr/lib/python3/dist-packages/qgis:/usr/share/qgis/python/qgis

WORKDIR /QGIS/build
RUN ccache -M 16G

# Run supervisor
CMD ["/bin/sh", "-c", "ninja -j4 install && /bin/sh"]
# CMD ["ninja", "-j4", "install"]


# CMD ["sh", "-c", "make && make install && ctest --output-on-failure -V"]