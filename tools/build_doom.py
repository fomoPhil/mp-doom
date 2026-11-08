#!/usr/bin/env python3
"""Build pinned doomgeneric for the Mac smoke test and MPC Linux ARM64."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from build_support import check_static_arm64, verify_upstream

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'e257a09527062bfb986b6e5e0780839b6cbd5b71'


def main():
    upstream = ROOT / 'upstream/doomgeneric'
    verify_upstream(ROOT)
    target = 'host-music' if '--test-music' in sys.argv else 'host-save' if '--test-save' in sys.argv else 'host-input' if '--test-input' in sys.argv else 'host' if '--host' in sys.argv else 'mpc'
    build = ROOT / 'build/doom' / target
    build.mkdir(parents=True, exist_ok=True)
    source = upstream / 'doomgeneric'
    makefile = (source / 'Makefile').read_text()
    names = re.search(r'^SRC_DOOM = (.*)$', makefile, re.M).group(1).split()
    files = [source / name.replace('.o', '.c') for name in names if name != 'doomgeneric_xlib.o']
    files += [ROOT / 'doom/mpc_port.c', ROOT / 'doom/mpc_save.c', ROOT / 'doom/mpc_surface.c']
    # Patch only the generated copy, retaining the pinned upstream checkout.
    original = (source / 'g_game.c').read_text()
    replacements = {
        '#include <string.h>': '#include <string.h>\n#include <errno.h>\n#include "mpc_save.h"\n#include "mpc_name_keys.h"',
        '    fclose(save_stream);\n\n    if (recovery_savegame_file != NULL)':
            '    if (mpc_save_close(save_stream, savegame_error) < 0)\n'
            '        I_Error("Failed to flush savegame: %s", strerror(errno));\n\n'
            '    if (recovery_savegame_file != NULL)',
        '    remove(savegame_file);\n    rename(temp_savegame_file, savegame_file);':
            '    if (mpc_save_replace(temp_savegame_file, savegame_file) < 0)\n'
            '        I_Error("Failed to commit savegame: %s", strerror(errno));',
        'recovery_savegame_file = M_TempFile("recovery.dsg");':
            'recovery_savegame_file = M_StringDuplicate("./recovery.dsg");',
        'mousex = ev->data2*(mouseSensitivity+5)/10;':
            'mousex += ev->data2*(mouseSensitivity+5)/10; /* Sum relative encoder events per tic. */',
        'if (gamekeydown[key_fire] || mousebuttons[mousebfire]':
            'if (gamekeydown[key_fire] || gamekeydown[MPC_KEY_ENCODER_PUSH] || mousebuttons[mousebfire]',
        'static boolean  gamekeydown[NUMKEYS];':
            'static boolean  gamekeydown[NUMKEYS];\n#include "mpc_run.h"',
        'speed = key_speed >= NUMKEYS':
            'speed = mpc_run_enabled || key_speed >= NUMKEYS',
        'boolean G_Responder (event_t* ev) \n{ ':
            'boolean G_Responder (event_t* ev) \n{ \n'
            '    if (ev->type == ev_keyup && ev->data1 == KEY_RSHIFT)\n'
            '        return MPC_RunRespond(ev); /* Release even on title/demo screens. */',
        '    switch (ev->type) \n    { \n      case ev_keydown:':
            '    if (MPC_RunRespond(ev)) return true;\n\n    switch (ev->type) \n    { \n      case ev_keydown:',
    }
    for before, after in replacements.items():
        if original.count(before) != 1:
            raise ValueError('Unexpected upstream save implementation: ' + before)
        original = original.replace(before, after)
    game_copy = build / 'g_game.c'
    game_copy.write_text(original)
    files = [game_copy if file.name == 'g_game.c' else file for file in files]
    # Encoder save-name editor stays in our overlay; upstream remains untouched.
    original = (source / 'm_menu.c').read_text()
    replacements = {
        '#include "m_menu.h"': '#include "m_menu.h"\n#include "mpc_input_logic.h"\n'
            'static void MPC_NameBegin(void);\nstatic void MPC_NameDraw(void);\n'
            'static void MPC_HelpDraw(void);\nstatic void MPC_HelpBegin(void);\nstatic void MPC_HelpNext(void);',
        '    saveCharIndex = strlen(savegamestrings[choice]);':
            '    saveCharIndex = strlen(savegamestrings[choice]);\n    MPC_NameBegin();',
        '// These keys evaluate to a "null" key in Vanilla Doom':
            '#include "mpc_name_menu.h"\n#include "mpc_help.h"\n#include "mpc_menu_encoder.h"\n#include "mpc_menu_labels.h"\n\n// These keys evaluate to a "null" key in Vanilla Doom',
        '\tif (name[0])\n\t{\n\t    V_DrawPatchDirect (x, y, W_CacheLumpName(name, PU_CACHE));':
            '\tif (currentMenu == &OptionsDef && i == mousesens)\n'
            '\t    MPC_DrawKnobLookLabel(x, y + 1);\n'
            '\telse if (name[0])\n\t{\n\t    V_DrawPatchDirect (x, y, W_CacheLumpName(name, PU_CACHE));',
        'boolean M_Responder (event_t* ev)\n{':
            'boolean M_Responder (event_t* ev)\n{\n'
            '    event_t translated;\n    static struct back_binding back;\n'
            '    if ((ev->type == ev_keydown || ev->type == ev_keyup) && ev->data1 == KEY_F12) {\n'
            '        translated = *ev;\n'
            '        translated.data1 = control_back(&back, ev->type == ev_keydown, KEY_F12,\n'
            '            menuactive, currentMenu == &MainDef, saveStringEnter, messageToPrint);\n'
            '        translated.data2 = translated.data1;\n        ev = &translated;\n    }\n'
            '    if (MPC_HelpRespond(ev)) return true;\n'
            '    if (MPC_MenuEncoderRespond(ev)) return true;\n'
            '    if (MPC_NameRespond(ev)) return true;',
        '    // DRAW SKULL\n':
            '    if (MPC_HelpActive() || MPC_MenuEncoderDraw()) return;\n\n    // DRAW SKULL\n',
        '    inhelpscreens = false;':
            '    inhelpscreens = false;\n    if (menuactive && saveStringEnter) { MPC_NameDraw(); return; }',
        '    DEH_snprintf(endstring, sizeof(endstring), "%s\\n\\n" DOSY,\n                 DEH_String(M_SelectEndMessage()));':
            '    M_StringCopy(endstring, "RETURN TO THE MPC?\\n\\n(PRESS PLUS (Y) TO EXIT TO MPC.)", sizeof(endstring));',
    }
    for before, after in replacements.items():
        if original.count(before) != 1:
            raise ValueError('Unexpected upstream menu implementation: ' + before)
        original = original.replace(before, after)
    start = original.index('    // Save Game string input\n')
    end = original.index('    // Take care of any messages that need input\n', start)
    original = original[:start] + original[end:]
    from doom_help_sources import patch_help
    original = patch_help(ROOT, build, original)
    menu_copy = build / 'm_menu.c'
    menu_copy.write_text(original)
    files = [menu_copy if file.name == 'm_menu.c' else file for file in files]
    # Restore the status bar on the first frame after the full-screen editor.
    original = (source / 'st_stuff.c').read_text()
    replacements = {
        '#include "st_stuff.h"': '#include "st_stuff.h"\n#include "mpc_status.h"',
        'static boolean\t\tst_firsttime;':
            'static boolean\t\tst_firsttime;\n\n'
            'void mpc_status_invalidate(void)\n{\n    st_firsttime = true;\n}',
    }
    for before, after in replacements.items():
        if original.count(before) != 1:
            raise ValueError('Unexpected upstream status implementation: ' + before)
        original = original.replace(before, after)
    status_copy = build / 'st_stuff.c'
    status_copy.write_text(original)
    files = [status_copy if file.name == 'st_stuff.c' else file for file in files]
    flags = ['-O2', '-g', '-D_DEFAULT_SOURCE', '-DNORMALUNIX', '-DLINUX', '-DDOOMGENERIC_RESX=320', '-DDOOMGENERIC_RESY=200', '-I' + str(source), '-I' + str(ROOT / 'doom'), '-I' + str(build)]
    flags += ['-fdebug-compilation-dir=.', '-ffile-prefix-map=' + str(ROOT) + '=.', '-fdebug-prefix-map=' + str(ROOT) + '=.']
    compiler = ['/usr/bin/clang']
    if target.startswith('host'):
        flags += ['-DMPC_HEADLESS']
        if target == 'host-input':
            flags += ['-DMPC_INPUT_BRIDGE_TEST']
            files.append(ROOT / 'doom/test_engine_input.c')
        elif target == 'host-save':
            files.append(ROOT / 'doom/test_persistence.c')
        elif target == 'host-music':
            files.append(ROOT / 'doom/test_engine_music.c')
    else:
        from doom_opening_sources import generate as generate_opening
        generate_opening(build)
        zig = os.environ.get('ZIG', str(ROOT / '.tools/zig-aarch64-macos-0.15.2/zig'))
        if subprocess.check_output([zig, 'version'], text=True).strip() != '0.15.2':
            raise ValueError('Zig 0.15.2 is required')
        compiler = [zig, 'cc', '-target', 'aarch64-linux-musl', '-static']
        files += [ROOT / 'doom/mpc_display.c', ROOT / 'doom/mpc_sound.c', ROOT / 'doom/mpc_input.c']
    if target in ('mpc', 'host-music'):
        flags += ['-DFEATURE_SOUND']
        patched = build / 'i_sound.c'
        original = (source / 'i_sound.c').read_text()
        assert original.count('#include <SDL_mixer.h>') == 1
        patched.write_text(original.replace('#include <SDL_mixer.h>', '/* Unused SDL include removed: MPC uses raw PCM. */'))
        files = [patched if file.name == 'i_sound.c' else file for file in files]
        from doom_music_sources import sources as music_sources
        music_files, music_flags = music_sources(ROOT, build)
        files += music_files
        flags += music_flags
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(ROOT / '.tools/zig-cache'), ZIG_LOCAL_CACHE_DIR=str(build / 'zig-cache'))

    def compile_file(file):
        obj = build / (file.stem + '.o')
        extra = ['-Dmain=mpc_headless_entry'] if target in ('host-input', 'host-save', 'host-music') and file.stem == 'mpc_port' else []
        result = subprocess.run([*compiler, *flags, *extra, '-c', str(file), '-o', str(obj)], env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(str(file) + '\n' + result.stdout)
        return str(obj)

    with ThreadPoolExecutor(max_workers=4) as pool:
        objects = list(pool.map(compile_file, files))
    output = build / 'mpc-doom'
    subprocess.run([*compiler, *objects, '-o', str(output), '-lm', '-pthread', *(['-s'] if target == 'mpc' else [])], env=env, check=True)
    if target == 'mpc':
        check_static_arm64(output)
    info = {'engine_revision': REVISION, 'target': target, 'sha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'bytes': output.stat().st_size,
            'help_content_sha256': hashlib.sha256((ROOT / 'doom/help_pages.json').read_bytes()).hexdigest(),
            'local_sources': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT / 'doom').glob('*.[ch]')},
            'patch': 'Build copies only: checked durable saves/RAM recovery; summed relative turn events; independent encoder push fire; session SHIFT run toggle; MPC help pages; encoder menus/sliders/name editor/contextual STOP; status redraw; unused SDL include removed for MPC',
            'build_tools_sha256': {name: hashlib.sha256((ROOT / 'tools' / name).read_bytes()).hexdigest()
                                   for name in ('build_doom.py', 'doom_music_sources.py', 'doom_help_sources.py', 'doom_opening_sources.py', 'build_support.py')},
            'generated_sources_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.parent == build},
            'flags': [flag.replace(str(ROOT), '.') for flag in flags],
            'link_flags': ['-lm', '-pthread'] + (['-s'] if target == 'mpc' else [])}
    if target == 'mpc':
        info['generated_sources_sha256']['mpc_opening_pixels.inc'] = hashlib.sha256((build / 'mpc_opening_pixels.inc').read_bytes()).hexdigest()
    if target in ('mpc', 'host-music'):
        from doom_music_sources import REVISION as MUSIC_REVISION
        info['music_revision'] = MUSIC_REVISION
        info['music'] = 'Original WAD MUS + GENMIDI; Chocolate Doom/Nuked OPL, 9-voice Sound Blaster FM, memory-only MIDI parser'
        info['music_driver_sha256'] = hashlib.sha256((build / 'mpc_oplmusic.inc').read_bytes()).hexdigest()
    (build / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
    subprocess.run(['/usr/bin/file', str(output)], check=True)
    print(json.dumps(info, indent=2))


if __name__ == '__main__':
    main()
