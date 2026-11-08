/* Full-engine input regression, linked against the same pinned DOOM sources. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "doomgeneric.h"
#include "doomkeys.h"
#include "d_event.h"
#include "g_game.h"
#include "doomstat.h"
#include "mpc_input_logic.h"
#include "i_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "st_stuff.h"

void D_ProcessEvents(void);
void M_Drawer(void);
void D_Display(void);
struct menu;
extern struct menu MainDef, EpiDef, NewDef, OptionsDef, SoundDef, LoadDef, SaveDef, ReadDef1, ReadDef2;
extern struct menu *currentMenu;
extern short itemOn;
extern int messageToPrint, saveStringEnter, mouseSensitivity, screenblocks, showMessages, detailLevel;
extern char *messageString;
extern int sfxVolume, musicVolume;
extern char savegamestrings[10][24];
char *P_SaveGameFile(int slot);
static struct controls menu_controls;

// Hardware providers are replaced; DG_GetKey and I_GetEvent are the production path.
int mpc_input_key(int *down, unsigned char *key) { return control_pop(&menu_controls,down,key); }
int mpc_input_turn(void) { int turn=menu_controls.turn; menu_controls.turn=0; return turn; }
int mpc_input_volume(void) { return control_take_volume(&menu_controls); }
int mpc_input_quit(void) { return menu_controls.quit; }
void mpc_sound_set_master_volume(int value) { (void)value; }
void I_GetEvent(void);

static void drain(void)
{
    do { I_GetEvent(); D_ProcessEvents(); } while(menu_controls.count);
}

static void edge(int down, int key)
{
    event_t event = {.type=down ? ev_keydown : ev_keyup, .data1=key, .data2=key};
    D_PostEvent(&event);
    D_ProcessEvents();
}

static int side(void)
{
    ticcmd_t cmd;
    G_BuildTiccmd(&cmd, 0);
    return cmd.sidemove;
}

static void midi_tap(int status, int note)
{
    unsigned char message[] = {status,note,127};
    for(int release=0;release<2;++release) {
        if(release) { message[0]=status==0x99?0x89:0x80; message[2]=0; }
        control_message(&menu_controls,message);
        drain();
    }
    D_Display();
}
static void encoder(int delta)
{
    unsigned char message[]={0xB0,100,delta<0?128+delta:delta};
    control_message(&menu_controls,message);
    drain(); D_Display();
}
static void midi_edge(int status, int note, int down)
{
    unsigned char message[]={(unsigned char)(down?status:status-0x10),(unsigned char)note,(unsigned char)(down?127:0)};
    control_message(&menu_controls,message);
    drain();
}
static int attacking(void)
{
    ticcmd_t command;
    G_BuildTiccmd(&command,0);
    return !!(command.buttons & BT_ATTACK);
}
static void screenshot(const char *path)
{
    byte *palette=W_CacheLumpName("PLAYPAL",PU_CACHE);
    FILE *out=fopen(path,"wb"); assert(out);
    fprintf(out,"P6\n320 200\n255\n");
    for(int i=0;i<320*200;++i) assert(fwrite(palette+I_VideoBuffer[i]*3,1,3,out)==3);
    assert(!fclose(out));
}
static void assert_status_restored(const char *transition)
{
    // button()/encoder() have rendered the first post-editor frame via D_Display.
    // Compare it immediately with the engine's own complete status repaint.
    byte actual[320*32]; char path[96];
    memcpy(actual,I_VideoBuffer+320*168,sizeof(actual));
    snprintf(path,sizeof(path),"encoder-%s-actual.ppm",transition); screenshot(path);
    ST_Drawer(false,true);
    snprintf(path,sizeof(path),"encoder-%s-reference.ppm",transition); screenshot(path);
    int differences=0;
    for(unsigned i=0;i<sizeof(actual);++i) differences+=actual[i]!=I_VideoBuffer[320*168+i];
    printf("STATUS_BAR_RESTORE %s differences=%d\n",transition,differences); fflush(stdout);
    assert(differences==0 && "name editor must not leave pixels on the gameplay status bar");
}
static void button(int note) { midi_tap(0x90,note); }
static void pad(int index) { midi_tap(0x99,mpc_pad_notes[index-1]); }
static void select_item(int index)
{
    for(int i=0; itemOn!=index && i<32; ++i) pad(6);
    assert(itemOn==index);
}
static void main_menu(int index)
{
    if(menuactive) button(7);
    button(7);
    assert(menuactive && currentMenu==&MainDef);
    select_item(index);
    button(82);
}
static void ticks(void)
{
    for(int i=0;i<32;++i) doomgeneric_Tick();
}
static void skills_menu(int episode)
{
    main_menu(0); assert(currentMenu==&EpiDef);
    select_item(episode-1);
    button(82); assert(currentMenu==&NewDef);
}
static void start_game(int episode, int skill)
{
    skills_menu(episode);
    select_item(skill);
    button(82);
    assert(!messageToPrint && !menuactive);
    ticks();
    assert(usergame && gamestate==GS_LEVEL);
    assert(gameepisode==episode && gamemap==1 && gameskill==(skill_t)skill);
}
static void slider_limits(int *value, int minimum, int maximum)
{
    int old=*value;
    for(int i=0;i<32;++i) pad(9);
    assert(*value==minimum);
    pad(9); assert(*value==minimum);
    for(int i=0;i<32;++i) pad(11);
    assert(*value==maximum);
    pad(11); assert(*value==maximum);
    for(int i=0;*value!=old && i<32;++i) pad(*value<old?11:9);
    assert(*value==old);
}
static void wrap_six_slots(void)
{
    select_item(0);
    pad(10); assert(itemOn==5);
    pad(6); assert(itemOn==0);
}
static void stop_to(struct menu *parent)
{
    button(81);
    assert(menuactive && currentMenu==parent && !menu_controls.quit);
}

static void open_slider(int sound, int index)
{
    main_menu(1);
    if(sound) { select_item(7); button(82); assert(currentMenu==&SoundDef); }
    select_item(index);
}
static void check_slider_adjustment(int sound,int index,int *value,int low,int high,const char *name)
{
    open_slider(sound,index); int original=*value;
    button(82); assert(*value==original && "selecting a slider must not change its value");
    char path[96]; snprintf(path,sizeof(path),"encoder-%s-adjust.ppm",name); screenshot(path);
    encoder(-63); assert(*value==low && itemOn==index);
    encoder(1); assert(*value==low+1 && itemOn==index);
    encoder(63); assert(*value==high && itemOn==index);
    encoder(1); assert(*value==high);
    encoder(original-high); assert(*value==original);
    button(111); assert(*value==original); // Push finishes adjustment.
    snprintf(path,sizeof(path),"encoder-%s-done.ppm",name); screenshot(path);
    encoder(1); assert(itemOn!=index && *value==original);
    select_item(index); button(111); assert(*value==original); // Push enters too.
    button(82); assert(*value==original); // PLAY finishes too.
    encoder(-1); assert(itemOn!=index && *value==original);
    select_item(index); button(82); button(81);
    assert(currentMenu==(sound?&SoundDef:&OptionsDef) && itemOn==index); // STOP leaves adjustment.
    encoder(1); assert(itemOn!=index && *value==original);
    select_item(index); button(82); button(7); assert(!menuactive); // SEQ closes everything.
    open_slider(sound,index); encoder(1); assert(itemOn!=index && *value==original);
    printf("SLIDER_ADJUST_OK %s: no entry jump, bounds, turn, PLAY/push finish, STOP, SEQ\n",name);
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);
    for (int i=0;i<120;++i) doomgeneric_Tick();
    assert(side() == 0);
    struct controls c = {0};
    const unsigned char right_down[] = {0x99,46,127};
    const unsigned char right_up[] = {0x89,46,0};
    int down; unsigned char key;
    control_message(&c,right_down);
    assert(control_pop(&c,&down,&key)); edge(down,key);
    assert(side() > 0);
    edge(1,KEY_ESCAPE); edge(0,KEY_ESCAPE);
    control_message(&c,right_up);
    assert(control_pop(&c,&down,&key)); edge(down,key);
    edge(1,KEY_ESCAPE); edge(0,KEY_ESCAPE);
    assert(side() == 0);
    for (int i=0;i<100;++i) {
        control_message(&c,right_down); assert(control_pop(&c,&down,&key)); edge(down,key);
        assert(side() > 0);
        control_message(&c,right_up); assert(control_pop(&c,&down,&key)); edge(down,key);
        assert(side() == 0);
    }
    puts("ENGINE_INPUT_REGRESSION_OK: strafe releases after menu and repeated taps");
    // Exact production batching boundary: 63 turns plus a release overflowed
    // the engine event ring before the bounded DG_GetKey drain was added.
    control_message(&menu_controls,right_down); drain(); assert(side()>0);
    const unsigned char clockwise[]={0xB0,100,1}, anticlockwise[]={0xB0,100,127};
    for(int i=0;i<63;++i) control_message(&menu_controls,clockwise);
    control_message(&menu_controls,right_up); drain();
    assert(side()==0 && !menu_controls.quit);
    ticcmd_t single_turn,double_turn,opposite_turn;
    control_message(&menu_controls,clockwise); drain(); G_BuildTiccmd(&single_turn,0);
    control_message(&menu_controls,clockwise); control_message(&menu_controls,clockwise);
    drain(); G_BuildTiccmd(&double_turn,0);
    assert(single_turn.angleturn<0 && double_turn.angleturn==2*single_turn.angleturn);
    control_message(&menu_controls,clockwise); control_message(&menu_controls,anticlockwise);
    drain(); G_BuildTiccmd(&opposite_turn,0); assert(opposite_turn.angleturn==0);
    puts("INPUT_BATCHING_OK: 63-turn/release burst retained; aim sums same/opposite CCs");
    // Encoder and pad4 are independent held fire sources through the production
    // input/menu/game chain. Releasing either must not cancel the other.
    assert(!menuactive && !attacking());
    midi_edge(0x90,111,1); assert(attacking());
    encoder(1); ticcmd_t aiming_fire; G_BuildTiccmd(&aiming_fire,0);
    assert(aiming_fire.angleturn<0 && (aiming_fire.buttons & BT_ATTACK));
    midi_edge(0x99,mpc_pad_notes[3],1); assert(attacking());
    midi_edge(0x90,111,0); assert(attacking());
    midi_edge(0x99,mpc_pad_notes[3],0); assert(!attacking());
    midi_edge(0x99,mpc_pad_notes[3],1);
    midi_edge(0x90,111,1); assert(attacking());
    midi_edge(0x99,mpc_pad_notes[3],0); assert(attacking());
    midi_edge(0x90,111,0); assert(!attacking());
    // A release after opening a menu must clear gameplay's held state.
    midi_edge(0x90,111,1); assert(attacking());
    button(7); assert(menuactive);
    midi_edge(0x90,111,0); button(7);
    assert(!menuactive && !attacking());
    button(7); select_item(1); // Options selection never becomes a shot.
    midi_edge(0x90,111,1); assert(currentMenu==&OptionsDef && !attacking());
    button(7); assert(!menuactive && !attacking());
    midi_edge(0x90,111,0); assert(!attacking());
    // Even a release in the save-name editor must clear the original shot.
    midi_edge(0x90,111,1); assert(attacking());
    main_menu(3); select_item(5); button(82); assert(saveStringEnter);
    midi_edge(0x90,111,0); assert(!attacking());
    button(111); assert(!strcmp(savegamestrings[5],"A") && !attacking());
    button(81); button(7); assert(!menuactive && !attacking());
    puts("ENCODER_FIRE_OK: hold/release, aim, overlap, menu selection and editor releases");
    midi_edge(0x99,mpc_pad_notes[9],1);
    ticcmd_t walk,run,after_shift;
    G_BuildTiccmd(&walk,0); assert(walk.forwardmove>0);
    button(49); G_BuildTiccmd(&run,0);
    assert(run.forwardmove>walk.forwardmove && "SHIFT tap must keep running after release");
    assert(!strcmp(players[consoleplayer].message,"RUN ON"));
    midi_edge(0x90,49,1); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove && "second press turns run off even while held");
    edge(1,KEY_RSHIFT); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove && "repeated down cannot toggle twice");
    midi_edge(0x90,49,0); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove);
    assert(!strcmp(players[consoleplayer].message,"RUN OFF"));
    button(7); button(49); button(7); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove && "menu SHIFT must not toggle gameplay run");
    midi_edge(0x99,mpc_pad_notes[9],0);
    puts("RUN_TOGGLE_OK: tap on/off, release, repeat suppression, messages and menu context");
    assert(gamemode==retail || gamemode==shareware);
    int episodes=gamemode==retail?4:1;
    for(int episode=1;episode<=episodes;++episode) start_game(episode,0);
    puts("EPISODE_MENUS_OK: every available episode starts through pads and PLAY");
    if(gamemode==retail)
        puts("ULTIMATE_EPISODES_OK: all four episodes selected through pad/PLAY menu controls");
    start_game(1,0);
    pad(12); assert(automapactive);
    pad(12); assert(!automapactive);
    players[consoleplayer].weaponowned[wp_chaingun] = true;
    edge(1,control_pad_key(15));
    ticcmd_t weapon_command;
    G_BuildTiccmd(&weapon_command,0);
    assert((weapon_command.buttons & BT_CHANGE) &&
           ((weapon_command.buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT) == wp_chaingun);
    for(int i=0;i<120 && players[consoleplayer].readyweapon!=wp_chaingun;++i) doomgeneric_Tick();
    edge(0,control_pad_key(15));
    assert(players[consoleplayer].readyweapon == wp_chaingun);
    puts("PAD_SWAP_OK: pad12 toggles automap; held pad16 selects owned chaingun");
    for(int skill=0;skill<4;++skill) start_game(1,skill);

    // Nightmare is selected through its actual menu row, with every cancel key.
    for(int i=0;i<3;++i) {
        skills_menu(1); select_item(4); button(82); assert(messageToPrint);
        button(i==0?55:i==1?81:7);
        assert(!messageToPrint && gameskill==sk_hard && !menu_controls.quit);
    }
    skills_menu(1); select_item(4); button(82); assert(messageToPrint);
    button(54); assert(!messageToPrint);
    ticks(); assert(usergame && gameskill==sk_nightmare && gameepisode==1);
    start_game(1,0);

    // STOP must return to the parent of every reachable submenu.
    main_menu(0); stop_to(&MainDef);
    skills_menu(1); stop_to(&EpiDef); stop_to(&MainDef);
    main_menu(1); stop_to(&MainDef);
    button(81); assert(!menuactive && !menu_controls.quit);
    button(81); assert(menuactive && currentMenu==&MainDef);
    main_menu(2); assert(currentMenu==&LoadDef); stop_to(&MainDef);
    main_menu(3); assert(currentMenu==&SaveDef); stop_to(&MainDef);
    main_menu(4); assert(currentMenu==&ReadDef1); stop_to(&MainDef);

    main_menu(1);
    select_item(1); int old=showMessages; button(82); assert(showMessages!=old);
    button(82); assert(showMessages==old);
    select_item(2); old=detailLevel; button(82); assert(detailLevel!=old);
    button(82); assert(detailLevel==old);
    select_item(3); slider_limits(&screenblocks,3,11);
    select_item(5); slider_limits(&mouseSensitivity,0,9);
    select_item(7); button(82); assert(currentMenu==&SoundDef);
    select_item(0); slider_limits(&sfxVolume,0,15);
    select_item(2); slider_limits(&musicVolume,0,15);
    stop_to(&OptionsDef); stop_to(&MainDef);
    puts("OPTIONS_MENUS_OK: toggles, all slider bounds and STOP parent navigation");

    // Alternative navigation through actual MIDI -> DG_GetKey -> M_Responder.
    if(menuactive) button(7);
    button(7); select_item(0); // Main: New Game.
    encoder(1); assert(itemOn==1 && currentMenu==&MainDef);
    encoder(-1); assert(itemOn==0);
    encoder(-1); assert(itemOn==5); // Wrap in both directions.
    encoder(2); assert(itemOn==1);
    button(111); assert(currentMenu==&OptionsDef);
    select_item(0); encoder(1); assert(itemOn==1);
    encoder(1); assert(itemOn==2);
    encoder(1); assert(itemOn==3);
    int old_screen=screenblocks; encoder(1);
    assert(itemOn==5 && screenblocks==old_screen); // Skip spacer, don't adjust slider.
    encoder(1); assert(itemOn==7); // Skip second spacer.
    button(111); assert(currentMenu==&SoundDef);
    select_item(0); // Native menus retain their previous selection.
    encoder(1); assert(itemOn==2); encoder(-1); assert(itemOn==0);
    int old_sfx=sfxVolume; pad(9); assert(sfxVolume==old_sfx-1);
    button(111); assert(sfxVolume==old_sfx-1 && "selecting a slider must not change its value");
    encoder(1); assert(sfxVolume==old_sfx);
    button(111); // Finish adjustment before leaving this menu.
    stop_to(&OptionsDef); stop_to(&MainDef);
    select_item(0); button(111); assert(currentMenu==&EpiDef);
    select_item(0); button(111); assert(currentMenu==&NewDef);
    select_item(1); encoder(-1); assert(itemOn==0);
    button(111); ticks(); assert(!menuactive && gameskill==sk_baby);
    main_menu(2); select_item(0); encoder(-1); assert(itemOn==5);
    button(111); assert(currentMenu==&LoadDef && menuactive); // Empty load is harmless.
    stop_to(&MainDef);
    main_menu(3); select_item(4); encoder(1); assert(itemOn==5);
    button(111); assert(saveStringEnter);
    encoder(1); button(111); assert(!strcmp(savegamestrings[5],"B"));
    button(81); assert(!saveStringEnter && menuactive);
    assert_status_restored("menu-knob-name-cancel");
    stop_to(&MainDef);
    main_menu(5); assert(messageToPrint);
    encoder(8); assert(messageToPrint);
    button(111); assert(messageToPrint); // Enter cannot accidentally answer Y/N.
    button(55); assert(!messageToPrint && !menu_controls.quit);
    main_menu(4); button(111); assert(currentMenu==&ReadDef1);
    button(111); assert(currentMenu==&ReadDef1);
    button(111);
    assert(currentMenu==&MainDef);
    button(7); assert(!menuactive);
    puts("ENCODER_MENUS_OK: up/down/wrap/spacers, push=PLAY, sliders via pads, nested menus, safe prompts, names and help");

    check_slider_adjustment(1,0,&sfxVolume,0,15,"sfx");
    check_slider_adjustment(1,2,&musicVolume,0,15,"music");
    check_slider_adjustment(0,5,&mouseSensitivity,0,9,"sensitivity");
    check_slider_adjustment(0,3,&screenblocks,3,11,"screen");
    puts("SLIDER_WORKFLOW_OK: all four native sliders support select-adjust-finish");

    // Fresh run: every empty load slot is safe, and navigation wraps both ways.
    main_menu(2); assert(currentMenu==&LoadDef); wrap_six_slots();
    for(int slot=0;slot<6;++slot) {
        assert(access(P_SaveGameFile(slot),F_OK)!=0);
        select_item(slot); button(82);
        assert(menuactive && currentMenu==&LoadDef && itemOn==slot);
    }
    stop_to(&MainDef);
    main_menu(3); assert(currentMenu==&SaveDef); wrap_six_slots();
    select_item(0); button(82); assert(saveStringEnter);
    button(82);
    assert(saveStringEnter && currentMenu==&SaveDef && menuactive);
    assert(access(P_SaveGameFile(0),F_OK)!=0);
    button(81); assert(!saveStringEnter);

    // A large single raw-input batch retains order across several bounded drains.
    control_message(&menu_controls,right_down); drain(); assert(side()>0);
    main_menu(3); select_item(5);
    char empty_description[24]; memcpy(empty_description,savegamestrings[5],sizeof(empty_description));
    button(82); assert(saveStringEnter);
    for(int i=0;i<96;++i) control_message(&menu_controls,clockwise);
    const unsigned char push_down[]={0x90,111,127},push_up[]={0x80,111,0};
    control_message(&menu_controls,push_down); control_message(&menu_controls,push_up);
    control_message(&menu_controls,right_up); drain();
    assert(!strcmp(savegamestrings[5],"S") && !menu_controls.quit);
    button(81); assert(!saveStringEnter && !strcmp(savegamestrings[5],empty_description) && side()==0);
    puts("ENCODER_BATCH_ORDER_OK: 96 turns, push and held-pad release retain order");

    // Encoder selects a candidate; only pushing commits it. This is an unused slot.
    main_menu(3); select_item(5); button(82); assert(saveStringEnter);
    encoder(7); // A -> H
    assert(!savegamestrings[5][0]);
    button(111);
    assert(!strcmp(savegamestrings[5],"H"));
    button(81); assert(!saveStringEnter && menuactive);

    const int digit_pads[6]={13,14,15,16,1,2};
    for(int slot=0;slot<6;++slot) {
        main_menu(3); select_item(slot); button(82); assert(saveStringEnter);
        for(int i=0;i<24;++i) button(9);
        if(slot==0) {
            pad(13); pad(14); button(9); pad(15);
            assert(!strcmp(savegamestrings[0],"13"));
        } else {
            pad(digit_pads[slot]);
            assert(savegamestrings[slot][0]=='1'+slot && !savegamestrings[slot][1]);
        }
        players[consoleplayer].armorpoints=40+slot;
        button(82); assert(!saveStringEnter && !menuactive);
        ticks(); assert(access(P_SaveGameFile(slot),F_OK)==0);
        players[consoleplayer].armorpoints=0;
        main_menu(2); select_item(slot); button(82); ticks();
        assert(usergame && gamestate==GS_LEVEL);
        assert(players[consoleplayer].armorpoints==40+slot);
    }
    // Canceling a changed existing name restores its prior description.
    main_menu(3); select_item(0); button(82); assert(saveStringEnter);
    button(9); pad(14); assert(!strcmp(savegamestrings[0],"12"));
    button(81);
    assert(!saveStringEnter && !strcmp(savegamestrings[0],"13"));
    assert(currentMenu==&SaveDef && menuactive && !menu_controls.quit);
    stop_to(&MainDef);
    puts("SAVE_LOAD_MENUS_OK: all six slots, wrap, empty selections, real state restoration and edit cancel");

    // Full encoder naming flow on a disposable test slot, never the real card.
    main_menu(3); select_item(5); button(82);
    button(9); assert(!savegamestrings[5][0]);
    const int hangar_turns[]={7,-7,13,-7,-6,17};
    for(int i=0;i<6;++i) { encoder(hangar_turns[i]); button(111); }
    assert(!strcmp(savegamestrings[5],"HANGAR"));
    screenshot("encoder-name.ppm");
    button(55); encoder(1); button(111); // Replace R with S.
    assert(!strcmp(savegamestrings[5],"HANGAS"));
    button(55); button(9); // Delete A before cursor; retain S suffix.
    assert(!strcmp(savegamestrings[5],"HANGS"));
    button(54); button(9); // End -> erase S.
    assert(!strcmp(savegamestrings[5],"HANG"));
    encoder(-18); button(111); encoder(17); button(111);
    assert(!strcmp(savegamestrings[5],"HANGAR"));
    for(int i=0;i<40;++i) button(55);
    button(9); assert(!strcmp(savegamestrings[5],"HANGAR")); // Beginning is bounded.
    for(int i=0;i<40;++i) button(54);
    players[consoleplayer].armorpoints=88;
    button(82); assert(!menuactive && !saveStringEnter);
    assert_status_restored("after-save");
    ticks();
    FILE *saved=fopen(P_SaveGameFile(5),"rb"); assert(saved);
    char description[24]; assert(fread(description,1,24,saved)==24); fclose(saved);
    assert(!strcmp(description,"HANGAR"));
    players[consoleplayer].armorpoints=0;
    main_menu(2); select_item(5); button(82); ticks();
    assert(players[consoleplayer].armorpoints==88);
    main_menu(3); select_item(5); button(82);
    button(9); button(7);
    assert(!saveStringEnter && menuactive && !strcmp(savegamestrings[5],"HANGAR"));
    assert_status_restored("after-seq-cancel");
    button(82); for(int i=0;i<24;++i) button(9);
    encoder(-3); // A wraps left to SPACE.
    button(111); button(82);
    assert(saveStringEnter && menuactive && !strcmp(savegamestrings[5]," "));
    screenshot("encoder-empty-name.ppm");
    button(81); assert(!strcmp(savegamestrings[5],"HANGAR"));
    assert_status_restored("after-stop-cancel");
    button(82); for(int i=0;i<24;++i) button(9);
    encoder(8); // Narrow I: exercise the exact 23-character storage bound.
    for(int i=0;i<23;++i) button(111);
    assert(strlen(savegamestrings[5])==23);
    button(111); assert(strlen(savegamestrings[5])==23);
    screenshot("encoder-full-name.ppm");
    button(81); assert(!strcmp(savegamestrings[5],"HANGAR"));
    button(82); for(int i=0;i<24;++i) button(9);
    encoder(22); // Wide W: exercise the original slot's display width limit.
    for(int i=0;i<23;++i) button(111);
    assert(strlen(savegamestrings[5])==19);
    button(81); assert(!strcmp(savegamestrings[5],"HANGAR"));
    stop_to(&MainDef); button(81); assert(!menuactive);
    ticcmd_t before_push,after_push;
    G_BuildTiccmd(&before_push,0); button(111); G_BuildTiccmd(&after_push,0);
    assert(before_push.buttons==after_push.buttons && !menuactive);
    encoder(1); G_BuildTiccmd(&after_push,0); assert(after_push.angleturn<0);
    puts("ENCODER_NAMING_OK: choose/push, replace, backspace, cursor bounds, cancel, empty/full names, actual save/load, aim preserved");

    // The map also leaves its status bar outside the area it redraws.
    pad(12); assert(automapactive);
    main_menu(3); select_item(5); button(82); button(111); button(81);
    assert_status_restored("automap-cancel");
    stop_to(&MainDef); button(81); pad(12); assert(!automapactive);
    // A smaller gameplay view has a patterned border as well as the status bar.
    main_menu(1); select_item(3);
    while(screenblocks>3) pad(9);
    stop_to(&MainDef); button(81);
    main_menu(3); select_item(5); button(82); button(111); button(7);
    assert_status_restored("small-view-cancel");
    stop_to(&MainDef); button(81);
    main_menu(1); select_item(3);
    while(screenblocks<10) pad(11);
    stop_to(&MainDef); button(81);
    puts("EDITOR_REDRAW_OK: first-frame status bar after save, STOP/SEQ cancel, map and small view");

    // The same MPC guide replaces every original keyboard help page in both WADs.
    byte help_first[320*200],help_second[320*200],help_third[320*200];
    main_menu(4); assert(currentMenu==&ReadDef1); D_Display();
    memcpy(help_first,I_VideoBuffer,sizeof(help_first)); screenshot("encoder-mpc-help-1.ppm");
    button(82); assert(currentMenu==&ReadDef1);
    memcpy(help_second,I_VideoBuffer,sizeof(help_second)); screenshot("encoder-mpc-help-2.ppm");
    assert(memcmp(help_first,help_second,sizeof(help_first)));
    button(111); assert(currentMenu==&ReadDef1);
    memcpy(help_third,I_VideoBuffer,sizeof(help_third)); screenshot("encoder-mpc-help-3.ppm");
    assert(memcmp(help_second,help_third,sizeof(help_second)));
    encoder(-1); assert(!memcmp(help_second,I_VideoBuffer,sizeof(help_second)));
    pad(10); assert(!memcmp(help_first,I_VideoBuffer,sizeof(help_first)));
    encoder(-63); assert(!memcmp(help_first,I_VideoBuffer,sizeof(help_first)));
    pad(6); assert(!memcmp(help_second,I_VideoBuffer,sizeof(help_second)));
    encoder(63); assert(!memcmp(help_third,I_VideoBuffer,sizeof(help_third)));
    button(81); assert(!memcmp(help_second,I_VideoBuffer,sizeof(help_second)));
    button(81); assert(!memcmp(help_first,I_VideoBuffer,sizeof(help_first)));
    button(81); assert(currentMenu==&MainDef); assert_status_restored("help-back-to-menu");
    main_menu(4); assert(!memcmp(help_first,I_VideoBuffer,sizeof(help_first)));
    button(82); button(111); button(82); assert(currentMenu==&MainDef);
    assert_status_restored("help-done-to-menu");
    button(7); assert(!menuactive && !attacking());
    midi_edge(0x90,111,1); assert(attacking());
    midi_edge(0x90,49,1); // RUN ON, then release while help owns presses.
    main_menu(4); midi_edge(0x90,111,0); midi_edge(0x90,49,0);
    assert(!attacking()); button(7); assert(!menuactive);
    assert_status_restored("help-seq-close");
    button(49); // RUN OFF proves the release reached gameplay.
    midi_edge(0x99,mpc_pad_notes[9],1); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove);
    midi_edge(0x99,mpc_pad_notes[9],0);
    main_menu(4); button(49); button(7); // SHIFT inside help has no gameplay effect.
    midi_edge(0x99,mpc_pad_notes[9],1); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove);
    midi_edge(0x99,mpc_pad_notes[9],0);
    pad(12); assert(automapactive); main_menu(4); button(7);
    assert_status_restored("help-automap-close"); pad(12); assert(!automapactive);
    if(gamemode==shareware) {
        main_menu(0); select_item(1); button(82); assert(messageToPrint);
        button(82); assert(!messageToPrint);
    }
    puts("MPC_HELP_OK: all3pages, knob/pads/PLAY/push/STOP/SEQ, bounds, reopening, held releases and clean status");
    assert(!menuactive);
    midi_edge(0x90,49,1); // Hold SHIFT while End Game returns to the title.
    main_menu(1); select_item(0); button(82); button(54); ticks();
    assert(!usergame && gamestate==GS_DEMOSCREEN);
    midi_edge(0x90,49,0);
    start_game(1,0); button(49); // First fresh tap must turn the retained RUN ON off.
    midi_edge(0x99,mpc_pad_notes[9],1); G_BuildTiccmd(&after_shift,0);
    assert(after_shift.forwardmove==walk.forwardmove && "title-screen release must not swallow the next run toggle");
    midi_edge(0x99,mpc_pad_notes[9],0);
    puts("RUN_TITLE_RELEASE_OK: held run key releases on title, first new-game tap works");

    // End Game uses the real Options row and must leave a canceled game intact.
    for(int i=0;i<3;++i) {
        main_menu(1); select_item(0); button(82); assert(messageToPrint);
        button(i==0?55:i==1?81:7);
        assert(!messageToPrint && usergame && gamestate==GS_LEVEL && !menu_controls.quit);
    }
    main_menu(1); select_item(0); button(82); assert(messageToPrint);
    button(54); assert(!messageToPrint); ticks();
    assert(!usergame && gamestate==GS_DEMOSCREEN);
    // The title/non-player state cannot save; its notice needs no keyboard.
    for(int i=0;i<3;++i) {
        main_menu(3); assert(messageToPrint);
        button(i==0?82:i==1?81:7);
        assert(!messageToPrint && !usergame && !menu_controls.quit);
    }
    main_menu(1); select_item(0); button(82);
    assert(!messageToPrint && currentMenu==&OptionsDef);
    stop_to(&MainDef);
    puts("END_GAME_MENUS_OK: Yes reaches title, No/STOP/SEQ keep game, cannot-save notice dismisses");

    // Quit is selected from the main menu, with No/Back before final Yes exits.
    for(int i=0;i<3;++i) {
        main_menu(5); assert(messageToPrint);
        assert(strstr(messageString,"PRESS PLUS (Y) TO EXIT TO MPC") && !strstr(messageString,"dos"));
        if(i==0) screenshot("encoder-quit-mpc.ppm");
        button(i==0?55:i==1?81:7);
        assert(!messageToPrint && !menu_controls.quit);
    }
    puts("ENGINE_MENU_AUDIT_OK: every singleplayer menu, all episode/skill choices, slider bounds, six-slot save/load, help and dialogs");
    main_menu(5); assert(messageToPrint); button(54);
    fputs("REGRESSION: PLUS/Yes failed to confirm the quit dialog\n", stderr);
    return 1;
}
