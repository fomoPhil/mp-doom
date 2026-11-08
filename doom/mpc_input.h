#pragma once
int mpc_input_open(void);
void mpc_input_close(void);
int mpc_input_key(int *down, unsigned char *key);
int mpc_input_turn(void);
int mpc_input_volume(void);
/* Poll only Power during startup, leaving MIDI input for the engine. */
void mpc_input_poll_power(void);
int mpc_input_quit(void);
