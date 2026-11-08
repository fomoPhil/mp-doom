"""Pinned GPL music sources and checked compatibility copies, no checkout edits."""
from pathlib import Path

REVISION = '35fb1372d10756ca27eca05665bd8a7cebc71c05'


def sources(root: Path, build: Path):
    upstream = root / 'upstream/chocolate-music'
    original = (upstream / 'src/i_oplmusic.c').read_text()
    # Use the existing mature voice allocation/instruments/tempo driver. Its
    # platform module/debug exports are replaced by our serialized wrapper.
    original = original[:original.index('const static snddevice_t music_opl_devices[]')]
    start = original.index('static boolean IsMid(')
    end = original.index('// Is the song playing?', start)
    original = original[:start] + '''// MPC: parse converted MIDI in memory; never write a temp file.
static void *I_OPL_RegisterSong(void *data, int len)
{
    if (!music_initialized || len <= 0) return NULL;
    if (len > 4 && !memcmp(data, "MThd", 4)) return MIDI_LoadMemory(data, len);
    MEMFILE *input = mem_fopen_read(data, len);
    MEMFILE *output = mem_fopen_write();
    midi_file_t *result = NULL;
    if (mus2mid(input, output) == 0) {
        void *buffer; size_t length;
        mem_get_buf(output, &buffer, &length);
        result = MIDI_LoadMemory(buffer, length);
    }
    mem_fclose(input);
    mem_fclose(output);
    return result;
}

''' + original[end:]
    original = original.replace('#include "midifile.h"', '#include "midifile.h"\n#define PACKED_STRUCT(...) struct __VA_ARGS__ PACKEDATTR\nmidi_file_t *MIDI_LoadMemory(void *data, size_t length);')
    (build / 'mpc_oplmusic.inc').write_text(original)
    original = (upstream / 'src/midifile.c').read_text()
    replacements = {
        '#include "midifile.h"': '#include "midifile.h"\n#define PACKED_STRUCT(...) struct __VA_ARGS__ PACKEDATTR\nstatic void *I_Realloc(void *ptr, size_t size) {\n    void *next = realloc(ptr, size);\n    if (size && !next) I_Error("MIDI allocation failed");\n    return next;\n}',
        'midi_file_t *MIDI_LoadFile(char *filename)': 'midi_file_t *MIDI_LoadMemory(void *data, size_t length)',
        'stream = M_fopen(filename, "rb");': 'stream = fmemopen(data, length, "rb");',
        'fprintf(stderr, "MIDI_LoadFile: Failed to open \'%s\'\\n", filename);': 'fprintf(stderr, "MIDI_LoadMemory: failed to open memory stream\\n");',
    }
    for before, after in replacements.items():
        if original.count(before) != 1:
            raise ValueError('Unexpected pinned music parser: ' + before)
        original = original.replace(before, after)
    original = original.replace('SDL_SwapBE16(', '__builtin_bswap16(').replace('SDL_SwapBE32(', '__builtin_bswap32(')
    # Both supported targets (Mac ARM64/x86_64 and Linux ARM64) are little endian.
    original = '#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__\n#error "Music parser requires little endian"\n#endif\n' + original
    midi = build / 'mpc_midifile.c'
    midi.write_text(original)
    return [root / 'doom/mpc_music.c', midi, upstream / 'opl/opl3.c', upstream / 'opl/opl_queue.c',
            root / 'upstream/doomgeneric/doomgeneric/mus2mid.c'], [
            '-I' + str(upstream / 'opl'), '-I' + str(upstream / 'src'), '-I' + str(build)]
