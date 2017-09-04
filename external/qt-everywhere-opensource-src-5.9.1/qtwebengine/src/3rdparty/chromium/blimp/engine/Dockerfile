FROM base:latest

RUN mkdir /tmp/engine

RUN useradd -ms /bin/bash blimp_user

# Permission is not set correctly on directories automatically created by
# Docker ADD/COPY (see bug https://github.com/docker/docker/issues/7511 and
# https://github.com/docker/docker/issues/1295). So working around this issue
# by adding the files to tmp directory and then moving it to right location.
ADD * /tmp/engine/
RUN mv /tmp/engine/chrome_sandbox /tmp/engine/chrome-sandbox && \
    mv /tmp/engine/third_party/blimp_fonts /tmp/engine/fonts && \
    cp -r /tmp/engine /engine && \
    chown -R blimp_user /engine && \
    rm -rf /tmp/engine

USER blimp_user
WORKDIR "/engine"

ENTRYPOINT ["/engine/start_engine.sh"]
