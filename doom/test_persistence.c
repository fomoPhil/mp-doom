// SPDX-License-Identifier: GPL-2.0-or-later
// Separate-process save/load verification; never included in the MPC game.
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#include "doomgeneric.h"
#include "doomstat.h"
#include "g_game.h"

char *P_SaveGameFile(int slot);

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);
    for (int i=0;i<120;++i) doomgeneric_Tick();
    const char *action=getenv("MPC_SAVE_ACTION");
    assert(action);
    if (!strcmp(action,"load")) {
        players[consoleplayer].armorpoints=0;
        G_LoadGame(P_SaveGameFile(5));
        for (int i=0;i<64;++i) doomgeneric_Tick();
        assert(players[consoleplayer].armorpoints==77);
        puts("PERSISTENT_CHECKPOINT_LOADED armor=77");
        return 0;
    }
    assert(!strcmp(action,"create") || !strcmp(action,"fail"));
    players[consoleplayer].armorpoints=!strcmp(action,"create") ? 77 : 99;
    players[consoleplayer].armortype=1;
    if (!strcmp(action,"fail")) {
        signal(SIGXFSZ,SIG_IGN);
        struct rlimit limit={512,512};
        assert(setrlimit(RLIMIT_FSIZE,&limit)==0);
    }
    G_SaveGame(5,"777");
    for (int i=0;i<64;++i) doomgeneric_Tick();
    assert(strcmp(action,"fail")); // Write failure must exit before reporting success.
    assert(access(P_SaveGameFile(5),F_OK)==0);
    puts("PERSISTENT_CHECKPOINT_CREATED armor=77");
    return 0;
}
