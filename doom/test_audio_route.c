#include <assert.h>
#include <stdio.h>
#include "mpc_audio_route.h"

struct device { int value[3], read_failure, write_failure, fail_after_write, writes; };
static int read_control(void *context, enum mpc_route_control control, int *value)
{
    struct device *device = context;
    if ((int)control == device->read_failure) return -1;
    *value = device->value[control];
    return 0;
}
static int write_control(void *context, enum mpc_route_control control, int value)
{
    struct device *device = context;
    assert(control == MPC_ROUTE_SPEAKER);
    if (device->write_failure) return -1;
    ++device->writes;
    device->value[control] = value;
    return device->fail_after_write ? -1 : 0;
}
static struct mpc_route_io backend(struct device *device)
{
    return (struct mpc_route_io){device, read_control, write_control};
}
int main(void)
{
    struct device device = {.read_failure=-1}; // Stock left speaker muted, no jacks.
    struct mpc_audio_route route;
    assert(mpc_route_begin(&route, backend(&device)) == 0);
    assert(device.value[MPC_ROUTE_SPEAKER] == 1);
    assert(mpc_route_restore(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    assert(mpc_route_begin(&route, backend(&device)) == 0);
    device.value[MPC_ROUTE_HEADPHONES] = 1;
    assert(mpc_route_refresh(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    device.value[MPC_ROUTE_HEADPHONES] = 0;
    assert(mpc_route_refresh(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 1);
    device.value[MPC_ROUTE_LINEOUT] = 1;
    assert(mpc_route_refresh(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    assert(mpc_route_restore(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    device.value[MPC_ROUTE_SPEAKER] = 1;
    assert(mpc_route_begin(&route, backend(&device)) == 0);
    assert(device.value[MPC_ROUTE_SPEAKER] == 0); // Line-out still inserted.
    assert(mpc_route_restore(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 1);
    device = (struct device){.read_failure=MPC_ROUTE_HEADPHONES};
    assert(mpc_route_begin(&route, backend(&device)) < 0);
    assert(device.writes == 0 && mpc_route_restore(&route) == 0);
    device = (struct device){.read_failure=-1, .write_failure=1};
    assert(mpc_route_begin(&route, backend(&device)) < 0);
    assert(device.value[MPC_ROUTE_SPEAKER] == 0 && mpc_route_restore(&route) < 0);
    device.write_failure = 0;
    assert(mpc_route_restore(&route) == 0);
    device = (struct device){.read_failure=-1};
    assert(mpc_route_begin(&route, backend(&device)) == 0);
    device.write_failure = 1;
    assert(mpc_route_restore(&route) < 0 && route.valid);
    device.write_failure = 0;
    assert(mpc_route_restore(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    // A write can reach hardware even when its readback fails.
    device = (struct device){.read_failure=-1, .fail_after_write=1};
    assert(mpc_route_begin(&route, backend(&device)) < 0 && device.value[MPC_ROUTE_SPEAKER] == 1);
    device.fail_after_write = 0;
    device.value[MPC_ROUTE_HEADPHONES] = 1;
    assert(mpc_route_refresh(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    device.value[MPC_ROUTE_HEADPHONES] = 0;
    assert(mpc_route_refresh(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 1);
    assert(mpc_route_restore(&route) == 0 && device.value[MPC_ROUTE_SPEAKER] == 0);
    puts("MPC audio route regression passed");
    return 0;
}
