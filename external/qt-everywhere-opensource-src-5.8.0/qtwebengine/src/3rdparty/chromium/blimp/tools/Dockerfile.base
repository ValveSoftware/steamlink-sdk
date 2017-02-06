# Blimp base Dockerfile. Should be used by all Dockerfiles used by Blimp to
# ensure test and release binaries all run under the same system environment.
FROM ubuntu:trusty

# Run the command below to update the lib list.
# ldd ./blimp_engine_app | grep usr/lib | awk '{print $3}' | xargs -n1 \
#   dpkg-query -S | awk -F: '{print $1}' | sort | uniq
RUN apt-get update && \
  apt-get install -yq libdrm2 libfontconfig1 libfreetype6 libgraphite2-3 \
  libharfbuzz0b libnspr4 libnss3 libstdc++6

# stunnel4 is needed for incoming TLS connections.
# wget is needed for crash reporting.
RUN apt-get update && apt-get install -yq stunnel4 wget

# gdb and strace are not strictly required, but having them around makes
# debugging easier.
RUN apt-get update && apt-get install -yq gdb strace