FROM debian:latest as build-basis

WORKDIR /build

# Install dependencies
RUN apt-get update && \
    apt-get install -y build-essential git automake autoconf libtool libmosquitto-dev

FROM build-basis as build-libmodbus

WORKDIR /libmodbus
    
RUN git clone https://github.com/stephane/libmodbus.git . && \
    bash autogen.sh && \
    ./configure && \
    make install

FROM build-libmodbus as build-s10m

WORKDIR /build_s10m
COPY . .

RUN make

FROM debian:latest as runtime

WORKDIR /app
COPY --from=build-s10m /build_s10m/s10m /usr/bin
COPY --from=build-libmodbus /usr/local/lib/* /usr/local/lib/

RUN apt-get update && \
    apt-get install -y libmosquitto-dev

CMD [ "s10m" ]
