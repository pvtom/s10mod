#ifndef __S10M_H_
#define __S10M_H_

#define CONFIG_FILE     ".config"

#define MODBUS_1_NR     110
#define MODBUS_1_OFFSET 0
#define MODBUS_2_NR     34
#define MODBUS_2_OFFSET 1000
#define REGISTER_41000  MODBUS_1_NR

#define BUFFER_SIZE     32
#define TOPIC_SIZE      128

#define F1              0
#define F01             1
#define F001            2

static void publish(char *topic, char *payload);
static void publish_number_if(int16_t *reg, int16_t *old, int format, int r, char *topic);
static void publish_string_if_changed(int16_t *reg, char *lst_value, int r, int length, char *topic);
static void publish_boolean(int16_t *reg, int r, int bit, int b, char *topic);

typedef struct _pool_t {
    int reg;
    int format;
    char topic[TOPIC_SIZE];
} pool_t;

pool_t pool[] = {
    { 83, F1, "battery/soc" },
    { 70, F1, "battery/power" },
    { 74, F1, "grid/power" },
    { 106, F1, "grid/power/L1" },
    { 107, F1, "grid/power/L2" },
    { 108, F1, "grid/power/L3" },
    { 72, F1, "home/power" },
    { 76, F1, "addon/power" },
    { 68, F1, "solar/power" },
    { 99, F001, "current/string_1" },
    { 100, F001, "current/string_2" },
    { 102, F1, "power/string_1" },
    { 103, F1, "power/string_2" },
    { 96, F1, "voltage/string_1" },
    { 97, F1, "voltage/string_2" },
    { REGISTER_41000 + 0, F1, "pvi/apparent_power/L1" },
    { REGISTER_41000 + 2, F1, "pvi/apparent_power/L2" },
    { REGISTER_41000 + 4, F1, "pvi/apparent_power/L3" },
    { REGISTER_41000 + 6, F1, "pvi/active_power/L1" },
    { REGISTER_41000 + 8, F1, "pvi/active_power/L2" },
    { REGISTER_41000 + 10, F1, "pvi/active_power/L3" },
    { REGISTER_41000 + 12, F1, "pvi/reactive_power/L1" },
    { REGISTER_41000 + 14, F1, "pvi/reactive_power/L2" },
    { REGISTER_41000 + 16, F1, "pvi/reactive_power/L3" },
    { REGISTER_41000 + 18, F01, "pvi/voltage/L1" },
    { REGISTER_41000 + 19, F01, "pvi/voltage/L2" },
    { REGISTER_41000 + 20, F01, "pvi/voltage/L3" },
    { REGISTER_41000 + 21, F001, "pvi/current/L1" },
    { REGISTER_41000 + 22, F001, "pvi/current/L2" },
    { REGISTER_41000 + 23, F001, "pvi/current/L3" },
    { REGISTER_41000 + 24, F001, "grid/frequency" },
    { REGISTER_41000 + 25, F1, "pvi/power/string_1" },
    { REGISTER_41000 + 26, F1, "pvi/power/string_2" },
    { REGISTER_41000 + 28, F01, "pvi/voltage/string_1" },
    { REGISTER_41000 + 29, F01, "pvi/voltage/string_2" },
    { REGISTER_41000 + 31, F001, "pvi/current/string_1" },
    { REGISTER_41000 + 32, F001, "pvi/current/string_2" }
};

#endif
