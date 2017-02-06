FROM base:latest

RUN mkdir /engine

RUN useradd -ms /bin/bash blimp_user

# The glob below expands to all files, but does not add directories
# recursively.
ADD * /engine/
ADD gen/third_party/blimp_fonts /engine/fonts
RUN mv /engine/chrome_sandbox /engine/chrome-sandbox
RUN chown -R blimp_user /engine

USER blimp_user
WORKDIR "/engine"

ENTRYPOINT ["/engine/start_engine.sh"]
