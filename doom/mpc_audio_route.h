#pragma once

enum mpc_route_control { MPC_ROUTE_SPEAKER, MPC_ROUTE_HEADPHONES, MPC_ROUTE_LINEOUT };
struct mpc_route_io {
    void *context;
    int (*read_control)(void *, enum mpc_route_control, int *);
    int (*write_control)(void *, enum mpc_route_control, int);
};
struct mpc_audio_route {
    struct mpc_route_io io;
    int saved, current, valid;
};

static inline int mpc_route_refresh(struct mpc_audio_route *route)
{
    if (!route->valid) return -1;
    int headphones, lineout;
    if (route->io.read_control(route->io.context, MPC_ROUTE_HEADPHONES, &headphones) < 0 ||
        route->io.read_control(route->io.context, MPC_ROUTE_LINEOUT, &lineout) < 0) return -1;
    int speaker = !headphones && !lineout;
    if (speaker != route->current) {
        if (route->io.write_control(route->io.context, MPC_ROUTE_SPEAKER, speaker) < 0) {
            route->current = -1; // Hardware may have changed before readback failed.
            return -1;
        }
        route->current = speaker;
    }
    return 0;
}

static inline int mpc_route_begin(struct mpc_audio_route *route, struct mpc_route_io io)
{
    *route = (struct mpc_audio_route){.io=io};
    if (io.read_control(io.context, MPC_ROUTE_SPEAKER, &route->saved) < 0) return -1;
    route->current = route->saved;
    route->valid = 1;
    return mpc_route_refresh(route);
}

static inline int mpc_route_restore(struct mpc_audio_route *route)
{
    if (!route->valid) return 0;
    // A failed write/readback may still have changed hardware. Restore explicitly.
    if (route->io.write_control(route->io.context, MPC_ROUTE_SPEAKER, route->saved) < 0) return -1;
    route->current = route->saved;
    route->valid = 0;
    return 0;
}
