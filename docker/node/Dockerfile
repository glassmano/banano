FROM ubuntu:22.04 as builder

ARG COMPILER=gcc
ARG NANO_NETWORK=live

# Install build dependencies
COPY ./ci/prepare/linux /tmp/prepare
RUN /tmp/prepare/prepare.sh

COPY ./ /tmp/src
WORKDIR /tmp/src

#Define ARGs for ci/build-node.sh
ARG BUILD_TYPE=RelWithDebInfo
ARG NANO_TEST=OFF
ARG COVERAGE=OFF
ARG CMAKE_SANITIZER=""
ARG CI_TAG=DEV_BUILD
ARG CI_BUILD=OFF
ARG CI_VERSION_PRE_RELEASE=OFF
ADD ./ /tmp/src

# Build node
RUN ci/build-node.sh
RUN echo ${NANO_NETWORK} >/etc/nano-network

RUN \
cmake /tmp/src \
-DCI_BUILD=${CI_BUILD} \
-DCI_VERSION_PRE_RELEASE=${CI_VERSION_PRE_RELEASE} \
-DPORTABLE=1 \
-DACTIVE_NETWORK=banano_${NETWORK}_network

RUN \
make bananode -j $(nproc) && \
make banano_rpc -j $(nproc) && \
cd .. && \
echo ${NETWORK} >/etc/nano-network

FROM ubuntu:22.04

RUN groupadd --gid 1000 nanocurrency && \
    useradd --uid 1000 --gid nanocurrency --shell /bin/bash --create-home nanocurrency

COPY --from=0 /tmp/build/bananode /usr/bin
COPY --from=0 /tmp/build/banano_rpc /usr/bin
COPY --from=0 /tmp/src/api/ /usr/bin/api/
COPY --from=0 /etc/nano-network /etc
COPY docker/node/entry.sh /usr/bin/entry.sh
COPY docker/node/config /usr/share/nano/config
RUN chmod +x /usr/bin/entry.sh
RUN ldconfig

WORKDIR /root
USER root

ENV PATH="${PATH}:/usr/bin"
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/usr/bin/entry.sh bananode daemon -l"]
