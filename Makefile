CC=gcc
PRG=s10m
MODBUS_CFLAGS= -I/usr/local/include/modbus
MODBUS_LIBS= -lmodbus -Wl,-rpath=/usr/local/lib/

all: $(PRG)

$(PRG): clean
	$(CC) -O3 s10m.c $(MODBUS_CFLAGS) -lmosquitto $(MODBUS_LIBS) -o $@

clean:
	-rm $(PRG)
