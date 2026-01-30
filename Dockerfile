FROM ghcr.io/wiiu-env/devkitppc:20260126 AS final

COPY --from=ghcr.io/wiiu-env/libmocha:20260126 /artifacts $DEVKITPRO

RUN apt update && apt -y install xxd

RUN git clone https://github.com/StroopwafelCFW/libstroopwafel.git && \
    cd libstroopwafel && \
    make install && \
    cd .. && \
    rm -rf libstroopwafel

WORKDIR project
