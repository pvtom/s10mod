#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mosquitto.h>
#include <modbus.h>
#include "s10m.h"

static struct mosquitto *mosq = NULL;
static int bg;

static void publish(char *topic, char *payload)
{
        char tbuf[TOPIC_SIZE];
        sprintf(tbuf, "%s/%s", ROOT_TOPIC, topic);
        if (mosq && mosquitto_publish(mosq, NULL, tbuf, strlen(payload), payload, MQTT_QOS, MQTT_RETAIN)) {
                if (!bg) printf("MQTT connection lost\n");
                mosquitto_disconnect(mosq);
                mosquitto_destroy(mosq);
                mosq = NULL;
        }
        if (!bg) printf("MQTT: publish topic >%s< payload >%s<\n", tbuf, payload);
}

static void publish_if_changed(int16_t *reg, int16_t *old, int r, char *topic)
{
        char sbuf[32];
        if (old[r] != reg[r]) {
                old[r] = reg[r];
                sprintf(sbuf, "%d", reg[r]);
                publish(topic, sbuf);
        }
}

int main(int argc, char **argv)
{
        int j, n;
        int nb = 120;
        modbus_t *ctx = NULL;
        int rc;
        char sbuf[32];
        char manufacturer[32];
        char model[32];
        char serial_number[32];
        char firmware[32];
        char bstate[32];
        char gstate[32];

        int16_t *regs = malloc(nb * sizeof(int16_t));
        int16_t *olds = malloc(nb * sizeof(int16_t));
        for (j = 0; j < nb; j++) olds[j] = 65535;

        strcpy(manufacturer, "");
        strcpy(model, "");
        strcpy(serial_number, "");
        strcpy(firmware, "");
        strcpy(bstate, "");
        strcpy(gstate, "");

        j = bg = 0;
        while (j < argc) {
            if (!strcmp(argv[j], "-d")) bg = 1;
            j++;
        }

        printf("Connecting...\n");
        printf("E3DC system %s:%s (Modbus)\n", MODBUS_HOST, MODBUS_PORT);
        printf("MQTT broker %s:%i qos = %i retain = %s\n", MQTT_HOST, MQTT_PORT, MQTT_QOS, MQTT_RETAIN ? "true" : "false");
        printf("Fetching data every ");
        if (POLL_INTERVAL == 1) printf("second.\n"); else printf("%i seconds.\n", POLL_INTERVAL);
        printf("\n");

        if (bg) {
            printf("...working as a daemon.\n");
            pid_t pid, sid;
            pid = fork();
            if (pid < 0)
                exit(EXIT_FAILURE);
            if (pid > 0)
                exit(EXIT_SUCCESS);
            umask(0);
            sid = setsid();
            if (sid < 0)
                exit(EXIT_FAILURE);
            if ((chdir("/")) < 0)
                exit(EXIT_FAILURE);
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }

        mosquitto_lib_init();

        while (1) {
                // MQTT connection
                if (!mosq) {
                    mosq = mosquitto_new(NULL, true, NULL);
                    if (!bg) fprintf(stderr, "Connecting to MQTT broker %s:%i\n", MQTT_HOST, MQTT_PORT);
                    if (mosq) {
                        if (!mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 10)) {
                            if (!bg) fprintf(stderr, "MQTT: Connected successfully\n");
                        } else {
                            if (!bg) fprintf(stderr, "MQTT: Connection failed\n");
                            mosquitto_destroy(mosq);
                            mosq = NULL;
                            sleep(1);
                            continue;
                        }
                    }
                }

                // modbus connection
                if (ctx == NULL) {
                        ctx = modbus_new_tcp_pi(MODBUS_HOST, MODBUS_PORT);

                        if (!ctx) {
                                if (!bg) fprintf(stderr, "E3DC_MODBUS: Unable to allocate libmodbus context\n");
                                sleep(1);
                                continue;
                        }
                        if (modbus_connect(ctx) == -1) {
                                if (!bg) fprintf(stderr, "E3DC_MODBUS: Connection failed: %s\n",modbus_strerror(errno));
                                if (ctx) modbus_free(ctx);
                                ctx = NULL;
                                sleep(1);
                                continue;
                        }
                        modbus_set_debug(ctx, FALSE);
                        modbus_set_response_timeout(ctx, 1, 0); // 1 second timeout
                        modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK|MODBUS_ERROR_RECOVERY_PROTOCOL);

                        if (!bg) fprintf(stderr,"E3DC_MODBUS: Connected successfully\n");
                }

                rc = modbus_read_registers(ctx, 0, nb, &regs[1]);
                if (rc != nb) {
                        if (!bg) fprintf(stderr,"E3DC_MODBUS: ERROR modbus_read_registers (%d/%d)\n", rc, nb);
                        if (ctx) modbus_close(ctx);
                        if (ctx) modbus_free(ctx);
                        ctx = NULL;
                        sleep(1);
                        continue;
                }

                if (regs[1] != -7204) {
                        if (!bg) fprintf(stderr,"E3DC_MODBUS: Modbus mode is wrong (%x). Must be E3DC, not SUN_SPEC.\n", regs[1]);
                        sleep(POLL_INTERVAL);
                        continue;
                }

                if (olds[82] != regs[82]) {
                        sprintf(sbuf, "%d", MODBUS_GET_HIGH_BYTE(regs[82])+1);
                        publish("autarky", sbuf);
                        sprintf(sbuf, "%d", MODBUS_GET_LOW_BYTE(regs[82])+1);
                        publish("consumption", sbuf);
                        olds[82] = regs[82];
                }

                if (olds[85] != regs[85]) {
                        sprintf(sbuf, "%d", (regs[85]&8)>>3);
                        publish("battery/charging/limit", sbuf);
                        sprintf(sbuf, "%d", regs[85]&1);
                        publish("battery/charging/lock", sbuf);
                        sprintf(sbuf, "%d", (regs[85]&2)>>1);
                        publish("battery/discharging/lock", sbuf);
                        sprintf(sbuf, "%d", (regs[85]&4)>>2);
                        publish("emergency/ready", sbuf);
                        sprintf(sbuf, "%d", (regs[85]&16)>>4);
                        publish("grid/limit", sbuf);
                        olds[85] = regs[85];
                }

                publish_if_changed(regs, olds, 83, "battery/soc");

                if (olds[70] != regs[70]) {
                        publish_if_changed(regs, olds, 70, "battery/power");
                        if (regs[71] >= 0) {
                                if ((regs[70] == 0) && (regs[83] == 0)) {
                                        if (strcmp(bstate, "EMPTY")) {
                                                publish("battery/state", "EMPTY");
                                                strcpy(bstate, "EMPTY");
                                        }
                                } else if ((regs[70] == 0) && (regs[83] == 100)) {
                                        if (strcmp(bstate, "FULL")) {
                                                publish("battery/state", "FULL");
                                                strcpy(bstate, "FULL");
                                        }
                                } else if (regs[70] == 0) {
                                        if (strcmp(bstate, "PENDING")) {
                                                publish("battery/state", "PENDING");
                                                strcpy(bstate, "PENDING");
                                        }
                                } else {
                                        if (strcmp(bstate, "CHARGING")) {
                                                publish("battery/state", "CHARGING");
                                                strcpy(bstate, "CHARGING");
                                        }
                                }
                        } else {
                                if (strcmp(bstate, "DISCHARGING")) {
                                        publish("battery/state", "DISCHARGING");
                                        strcpy(bstate, "DISCHARGING");
                                }
                        }
                }

                if (olds[84] != regs[84]) {
                        if (regs[84] == 1) {
                                publish("emergency/mode", "ACTIVE");
                        } else if (regs[84] == 2) {
                                publish("emergency/mode", "INACTIVE");
                        } else {
                                publish("emergency/mode", "N/A");
                        }
                        olds[84] = regs[84];
                }

                n = 0;
                for (j = 52; j < 16+52; j++) {sbuf[n++] = MODBUS_GET_HIGH_BYTE(regs[j]); sbuf[n++] = MODBUS_GET_LOW_BYTE(regs[j]);}
                if (strcmp(sbuf, firmware)) {
                        publish("firmware", sbuf);
                        strcpy(firmware, sbuf);
                }

                if (olds[74] != regs[74]) {
                        publish_if_changed(regs, olds, 74, "grid/power");
                        if (regs[75] < 0) {
                                if (strcmp(gstate, "IN")) {
                                        publish("grid/state", "IN");
                                        strcpy(gstate, "IN");
                                }
                        } else {
                                if (strcmp(gstate, "OUT")) {
                                        publish("grid/state", "OUT");
                                        strcpy(gstate, "OUT");
                                }
                        }
                }

                publish_if_changed(regs, olds, 106, "grid/power/L1");
                publish_if_changed(regs, olds, 107, "grid/power/L2");
                publish_if_changed(regs, olds, 108, "grid/power/L3");

                publish_if_changed(regs, olds, 72, "home/power");

                n = 0;
                for (j = 4; j < 16+4; j++) {sbuf[n++] = MODBUS_GET_HIGH_BYTE(regs[j]); sbuf[n++] = MODBUS_GET_LOW_BYTE(regs[j]);}
                if (strcmp(sbuf, manufacturer)) {
                        publish("manufacturer", sbuf);
                        strcpy(manufacturer, sbuf);
                }

                n = 0;
                for (j = 20; j < 16+20; j++) {sbuf[n++] = MODBUS_GET_HIGH_BYTE(regs[j]); sbuf[n++] = MODBUS_GET_LOW_BYTE(regs[j]);}
                if (strcmp(sbuf, model)) {
                        publish("model", sbuf);
                        strcpy(model, sbuf);
                }

                n = 0;
                for (j = 36; j < 16+36; j++) {sbuf[n++] = MODBUS_GET_HIGH_BYTE(regs[j]); sbuf[n++] = MODBUS_GET_LOW_BYTE(regs[j]);}
                if (strcmp(sbuf, serial_number)) {
                        publish("serial_number", sbuf);
                        strcpy(serial_number, sbuf);
                }

                publish_if_changed(regs, olds, 68, "solar/power");

                if (olds[99] != regs[99]) {
                        sprintf(sbuf, "%.2f", 0.01*regs[99]);
                        publish("string_1/current", sbuf);
                        olds[99] = regs[99];
                }
                if (olds[100] != regs[100]) {
                        sprintf(sbuf, "%.2f", 0.01*regs[100]);
                        publish("string_2/current", sbuf);
                        olds[100] = regs[100];
                }

                publish_if_changed(regs, olds, 102, "string_1/power");
                publish_if_changed(regs, olds, 103, "string_2/power");
                publish_if_changed(regs, olds, 96, "string_1/voltage");
                publish_if_changed(regs, olds, 97, "string_2/voltage");

                sleep(POLL_INTERVAL);
        }
        if (ctx) modbus_close(ctx);
        if (ctx) modbus_free(ctx);
        if (regs) free(regs);
        if (olds) free(olds);

        mosquitto_lib_cleanup();

        exit(0);
}
