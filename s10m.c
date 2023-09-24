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
char root_topic[128];
int mqtt_qos = 0;
int mqtt_retain = 0;
int verbose = 0;

static void publish(char *topic, char *payload) {
    char tbuf[TOPIC_SIZE];

    sprintf(tbuf, "%s/%s", root_topic, topic);
    if (mosq && mosquitto_publish(mosq, NULL, tbuf, strlen(payload), payload, mqtt_qos, mqtt_retain)) {
        printf("MQTT connection lost\n");
        fflush(NULL);
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosq = NULL;
    }
    if (verbose) printf("MQTT: publish topic >%s< payload >%s<\n", tbuf, payload);
}

static void publish_number_if_changed(int16_t *reg, int16_t *old, int format, int r, char *topic) {
    char sbuf[BUFFER_SIZE];

    if (old[r] != reg[r]) {
        old[r] = reg[r];
        switch (format) {
            case F1: {
                sprintf(sbuf, "%d", reg[r]);
                break;
            }
            case F01: {
                sprintf(sbuf, "%.1f", 0.1 * reg[r]);
                break;
            }
            case F001: {
                sprintf(sbuf, "%.2f", 0.01 * reg[r]);
                break;
            }
        }
        publish(topic, sbuf);
    }
}

static void publish_string_if_changed(int16_t *reg, char *lst_value, int r, int length, char *topic) {
    char sbuf[BUFFER_SIZE];
    int j, i = 0;

    for (j = r; j < length + r; j++) {
        sbuf[i++] = MODBUS_GET_HIGH_BYTE(reg[j]);
        sbuf[i++] = MODBUS_GET_LOW_BYTE(reg[j]);
    }
    if (strcmp(sbuf, lst_value)) {
        publish(topic, sbuf);
        strcpy(lst_value, sbuf);
    }
}

static void publish_boolean(int16_t *reg, int r, int bit, int b, char *topic) {
    char sbuf[BUFFER_SIZE];

    sprintf(sbuf, "%s", ((reg[r] & b) >> bit)?"true":"false");
    publish(topic, sbuf);
}

int main(int argc, char **argv) {
    modbus_t *ctx = NULL;
    int rc, bg, i, j;
    char tbuf[TOPIC_SIZE];
    char sbuf[BUFFER_SIZE];
    char manufacturer[BUFFER_SIZE];
    char model[BUFFER_SIZE];
    char serial_number[BUFFER_SIZE];
    char firmware[BUFFER_SIZE];
    char bstate[BUFFER_SIZE];
    char gstate[BUFFER_SIZE];
    char key[128], value[128], line[256];
    int reg_nr[2], reg_delta[2], reg_offset[2], wallbox;
    char mqtt_host[128];
    char mqtt_user[128];
    char mqtt_password[128];
    char modbus_host[128];
    char modbus_port[128];
    int mqtt_port = 1883;
    int mqtt_auth = 0;
    int interval = 1;
    int force = 0;
    int pool_length = sizeof(pool) / sizeof(pool[0]);

    int16_t *regs = malloc((MODBUS_1_NR + MODBUS_2_NR) * sizeof(int16_t));
    int16_t *olds = malloc((MODBUS_1_NR + MODBUS_2_NR) * sizeof(int16_t));

    for (j = 0; j < (MODBUS_1_NR + MODBUS_2_NR); j++) olds[j] = 2 ^ 15 - 1;

    reg_nr[0] = MODBUS_1_NR;
    reg_delta[0] = 1;
    reg_offset[0] = MODBUS_1_OFFSET;
    reg_nr[1] = MODBUS_2_NR;
    reg_delta[1] = MODBUS_1_NR;
    reg_offset[1] = MODBUS_2_OFFSET;

    strcpy(modbus_host, "e3dc");
    strcpy(modbus_port, "502");
    strcpy(mqtt_host, "localhost");
    strcpy(root_topic, "s10");
    strcpy(mqtt_user, "");
    strcpy(mqtt_password, "");
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

    FILE *fp;

    fp = fopen(CONFIG_FILE, "r");
    if (!fp)
        printf("Config file %s not found. Using default values.\n", CONFIG_FILE);
    else {
        while (fgets(line, sizeof(line), fp)) {
            memset(key, 0, sizeof(key));
            memset(value, 0, sizeof(value));
            if (sscanf(line, "%127[^ \t=]=%127[^\n]", key, value) == 2) {
                if (!strcasecmp(key, "MODBUS_HOST"))
                    strcpy(modbus_host, value);
                else if (!strcasecmp(key, "MODBUS_PORT"))
                    strcpy(modbus_port, value);
                else if (!strcasecmp(key, "MQTT_HOST"))
                    strcpy(mqtt_host, value);
                else if (!strcasecmp(key, "MQTT_PORT"))
                    mqtt_port = atoi(value);
                else if (!strcasecmp(key, "MQTT_USER"))
                    strcpy(mqtt_user, value);
                else if (!strcasecmp(key, "MQTT_PASSWORD"))
                    strcpy(mqtt_password, value);
                else if (!strcasecmp(key, "MQTT_AUTH") && !strcasecmp(value, "true"))
                    mqtt_auth = 1;
                else if (!strcasecmp(key, "MQTT_RETAIN") && !strcasecmp(value, "true"))
                    mqtt_retain = 1;
                else if (!strcasecmp(key, "MQTT_QOS"))
                    mqtt_qos = abs(atoi(value));
                if (mqtt_qos > 2) mqtt_qos = 0;
                else if (!strcasecmp(key, "INTERVAL"))
                    interval = abs(atoi(value));
                else if (!strcasecmp(key, "ROOT_TOPIC"))
                    strncpy(root_topic, value, 24);
                else if (!strcasecmp(key, "FORCE") && !strcasecmp(value, "true"))
                    force = 1;
            }
        }
        fclose(fp);
    }

    printf("Connecting...\n");
    printf("E3DC system %s:%s (Modbus)\n", modbus_host, modbus_port);
    printf("MQTT broker %s:%i qos = %i retain = %s\n", mqtt_host, mqtt_port, mqtt_qos, mqtt_retain?"true":"false");
    printf("Fetching data every ");
    if (interval == 1) printf("second.\n"); else printf("%i seconds.\n", interval);
    printf("Force mode = %s\n", force?"true":"false");
    if (isatty(STDOUT_FILENO)) {
        printf("Stdout to terminal\n");
        verbose = 1;
    } else {
        printf("Stdout to pipe/file\n");
        bg = 0;
    }
    printf("\n");
    fflush(NULL);

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
            printf("Connecting to MQTT broker %s:%i\n", mqtt_host, mqtt_port);
            fflush(NULL);
            if (mosq) {
                if (mqtt_auth && strcmp(mqtt_user, "") && strcmp(mqtt_password, "")) mosquitto_username_pw_set(mosq, mqtt_user, mqtt_password);
                if (!mosquitto_connect(mosq, mqtt_host, mqtt_port, 10)) {
                    printf("MQTT: Connected successfully\n");
                    fflush(NULL);
                } else {
                    printf("MQTT: Connection failed\n");
                    fflush(NULL);
                    mosquitto_destroy(mosq);
                    mosq = NULL;
                    sleep(1);
                    continue;
                }
            }
        }

        // modbus connection
        if (ctx == NULL) {
            ctx = modbus_new_tcp_pi(modbus_host, modbus_port);

            if (!ctx) {
                printf("E3DC_MODBUS: Unable to allocate libmodbus context\n");
                fflush(NULL);
                sleep(1);
                continue;
            }
            if (modbus_connect(ctx) == -1) {
                printf("E3DC_MODBUS: Connection failed: %s\n", modbus_strerror(errno));
                fflush(NULL);
                if (ctx) modbus_free(ctx);
                ctx = NULL;
                sleep(1);
                continue;
            }
            modbus_set_debug(ctx, FALSE);
            modbus_set_response_timeout(ctx, 1, 0); // 1 second timeout
            modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL);

            printf("E3DC_MODBUS: Connected successfully\n");
            fflush(NULL);
        }

        if (force) for (j = 0; j < (MODBUS_1_NR + MODBUS_2_NR); j++) olds[j] = 2 ^ 15 - 1;

        for (i = 0; i < 2; i++) {
            rc = modbus_read_registers(ctx, reg_offset[i], reg_nr[i], &regs[reg_delta[i]]);
            if (rc != reg_nr[i]) {
                printf("E3DC_MODBUS: ERROR modbus_read_registers at %d (%d/%d)\n", reg_delta[i], rc, reg_nr[i]);
                fflush(NULL);
                if (ctx) modbus_close(ctx);
                if (ctx) modbus_free(ctx);
                ctx = NULL;
                sleep(1);
                continue;
            }
        }

        if (regs[1] != -7204) {
            printf("E3DC_MODBUS: Modbus mode is wrong (%x). Must be E3DC, not SUN_SPEC.\n", regs[1]);
            fflush(NULL);
            sleep(interval);
            continue;
        }

        if (olds[82] != regs[82]) {
            sprintf(sbuf, "%d", MODBUS_GET_HIGH_BYTE(regs[82]) + 1);
            publish("autarky", sbuf);
            sprintf(sbuf, "%d", MODBUS_GET_LOW_BYTE(regs[82]) + 1);
            publish("consumption", sbuf);
            olds[82] = regs[82];
        }

        if (olds[85] != regs[85]) {
            publish_boolean(regs, 85, 0, 1, "battery/charging/lock");
            publish_boolean(regs, 85, 1, 2, "battery/discharging/lock");
            publish_boolean(regs, 85, 2, 4, "emergency/ready");
            publish_boolean(regs, 85, 3, 8, "battery/weather_regulation");
            publish_boolean(regs, 85, 4, 16, "grid/limit");
            publish_boolean(regs, 85, 5, 32, "idle_period/charging/active");
            publish_boolean(regs, 85, 6, 64, "idle_period/discharging/active");
            olds[85] = regs[85];
        }

        if (regs[88] & 1) {
            publish_number_if_changed(regs, olds, F1, 78, "wallbox/total/power");
            publish_number_if_changed(regs, olds, F1, 80, "wallbox/solar/power");
        }

        for (wallbox = 0; wallbox < 8; wallbox++) {
            if ((regs[88 + wallbox] & 1) && (olds[88 + wallbox] != regs[88 + wallbox])) {
                sprintf(tbuf, "wallbox/%d/available", wallbox);
                publish_boolean(regs, 88 + wallbox, 0, 1, tbuf);
                sprintf(tbuf, "wallbox/%d/sun_mode", wallbox);
                publish_boolean(regs, 88 + wallbox, 1, 2, tbuf);
                sprintf(tbuf, "wallbox/%d/ready", wallbox);
                publish_boolean(regs, 88 + wallbox, 2, 4, tbuf);
                sprintf(tbuf, "wallbox/%d/charging", wallbox);
                publish_boolean(regs, 88 + wallbox, 3, 8, tbuf);
                sprintf(tbuf, "wallbox/%d/1phase", wallbox);
                publish_boolean(regs, 88 + wallbox, 12, 4096, tbuf);
                olds[88 + wallbox] = regs[88 + wallbox];
            }
        }

        if (olds[70] != regs[70]) {
            if (regs[71] >= 0) {
                if ((regs[70] == 0) && (regs[83] == 0)) {
                    if (strcmp(bstate, "EMPTY")) {
                        publish("battery/state", "EMPTY");
                        if (!force) strcpy(bstate, "EMPTY");
                    }
                } else if ((regs[70] == 0) && (regs[83] == 100)) {
                    if (strcmp(bstate, "FULL")) {
                        publish("battery/state", "FULL");
                        if (!force) strcpy(bstate, "FULL");
                    }
                } else if (regs[70] == 0) {
                    if (strcmp(bstate, "PENDING")) {
                        publish("battery/state", "PENDING");
                        if (!force) strcpy(bstate, "PENDING");
                    }
                } else {
                    if (strcmp(bstate, "CHARGING")) {
                        publish("battery/state", "CHARGING");
                        if (!force) strcpy(bstate, "CHARGING");
                    }
                }
            } else {
                if (strcmp(bstate, "DISCHARGING")) {
                    publish("battery/state", "DISCHARGING");
                    if (!force) strcpy(bstate, "DISCHARGING");
                }
            }
        }

        if (olds[84] != regs[84]) {
            if (regs[84] == 1)
                publish("emergency/mode", "ACTIVE");
            else if (regs[84] == 2)
                publish("emergency/mode", "INACTIVE");
            else if (regs[84] == 4)
                publish("emergency/mode", "CHECK_MOTOR_SWITCH");
            else
                publish("emergency/mode", "N/A");
            olds[84] = regs[84];
        }

        publish_string_if_changed(regs, &manufacturer[0], 4, 16, "manufacturer");
        publish_string_if_changed(regs, &model[0], 20, 16, "model");
        publish_string_if_changed(regs, &serial_number[0], 36, 16, "serial_number");
        publish_string_if_changed(regs, &firmware[0], 52, 16, "firmware");

        if (olds[74] != regs[74]) {
            if (regs[75] < 0) {
                if (strcmp(gstate, "IN")) {
                    publish("grid/state", "IN");
                    if (!force) strcpy(gstate, "IN");
                }
            } else {
                if (strcmp(gstate, "OUT")) {
                    publish("grid/state", "OUT");
                    if (!force) strcpy(gstate, "OUT");
                }
            }
        }

        for (i = 0; i < pool_length; i++) {
            publish_number_if_changed(regs, olds, pool[i].format, pool[i].reg, pool[i].topic);
        }

        sleep(interval);
    }
    if (ctx) modbus_close(ctx);
    if (ctx) modbus_free(ctx);
    if (regs) free(regs);
    if (olds) free(olds);

    mosquitto_lib_cleanup();

    exit(EXIT_SUCCESS);
}
