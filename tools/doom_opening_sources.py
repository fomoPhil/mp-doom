"""Generate the original MP DOOM opening artwork as XRGB pixels.

The 320x240 frame and type are ported from preview/mp-doom-opening-credit.html.
The two-line MP DOOM logo matches the philwoolley.com/mpdoom title (original
letterforms: beveled frames, tech metal over gold bricks split by a waveform),
redrawn here as hard-edged pixel art. No external image or font is needed.
"""
from pathlib import Path
import math

WIDTH, HEIGHT = 320, 240
GLYPHS = {
    'A': [14,17,17,31,17,17,17], 'B': [30,17,17,30,17,17,30], 'C': [15,16,16,16,16,16,15],
    'D': [30,17,17,17,17,17,30], 'E': [31,16,16,30,16,16,31], 'H': [17,17,17,31,17,17,17],
    'I': [31,4,4,4,4,4,31], 'L': [16,16,16,16,16,16,31], 'M': [17,27,21,21,17,17,17],
    'O': [14,17,17,17,17,17,14], 'P': [30,17,17,30,16,16,16], 'R': [30,17,17,30,20,18,17],
    'S': [15,16,16,14,1,1,30], 'T': [31,4,4,4,4,4,4], 'W': [17,17,17,21,21,27,17],
    'Y': [17,17,10,4,4,4,4], ' ': [0,0,0,0,0,0,0],
}


def pixels():
    image = [0] * (WIDTH * HEIGHT)

    def rect(x, y, width, height, color):
        for row in range(y, y + height):
            image[row * WIDTH + x:row * WIDTH + x + width] = [color] * width

    def text(value, y, sx, sy, colors, offset=0):
        left = (WIDTH - (len(value) * 6 - 1) * sx) // 2 + offset
        for index, char in enumerate(value):
            for row_index, row in enumerate(GLYPHS[char]):
                for col in range(5):
                    if row & (16 >> col):
                        rect(left + (index * 6 + col) * sx, y + row_index * sy,
                             sx, sy, colors[row_index % len(colors)])

    rect(0, 0, WIDTH, HEIGHT, 0x0c080a)
    for y in range(8, 232, 4):
        for x in range(8, 312, 4):
            rim = max(abs(x - 160) / 152, abs(y - 120) / 112)
            grain = ((x * 23 ^ y * 31 ^ x * y) >> 1) % 9
            # JavaScript Math.round rounds positive halves upward.
            rgb = [math.floor(v + 0.5) for v in (18 + rim * 29 + grain, 6 + rim * 8, 8 + rim * 8)]
            rect(x, y, 4, 4, rgb[0] << 16 | rgb[1] << 8 | rgb[2])
    rect(15, 16, 290, 2, 0x6d342b); rect(15, 222, 290, 2, 0x431e1a)
    rect(16, 18, 2, 204, 0x4d2424); rect(302, 18, 2, 204, 0x311518)
    for x in (24, 290):
        for y in (25, 210):
            rect(x, y, 6, 5, 0x140a0b); rect(x, y, 4, 2, 0x9a5541)
    for x in range(34, 90, 8):
        rect(x, 35, 5, 2, 0x713227); rect(320 - x - 5, 35, 5, 2, 0x713227)
    draw_logo(image)
    rect(75, 165, 170, 1, 0x813a2b); rect(104, 166, 112, 1, 0x32191b)
    text('BY PHIL WOOLLEY', 172, 2, 2, [0xf3d5aa])
    text('MPC SAMPLE PORT', 199, 1, 1, [0xcb967b])
    return image


# Logo letters in design units (each 100 tall). P, D and O have a counter (hole).
_M = [(12, 100), (0, 88), (0, 12), (12, 0), (34, 0), (50, 28), (66, 0), (88, 0), (100, 12),
      (100, 88), (88, 100), (74, 100), (74, 50), (58, 76), (42, 76), (26, 50), (26, 100)]
_P = ([(12, 100), (0, 88), (0, 12), (12, 0), (66, 0), (80, 14), (80, 52), (66, 66), (26, 66), (26, 100)],
      [(26, 22), (52, 22), (56, 26), (56, 40), (52, 44), (26, 44)])
_D = ([(12, 100), (0, 88), (0, 12), (12, 0), (60, 0), (84, 24), (84, 76), (60, 100)],
      [(26, 24), (50, 24), (58, 32), (58, 68), (50, 76), (26, 76)])
_O = ([(20, 0), (64, 0), (84, 20), (84, 80), (64, 100), (20, 100), (0, 80), (0, 20)],
      [(32, 24), (52, 24), (58, 30), (58, 70), (52, 76), (32, 76), (26, 70), (26, 30)])
# (outline, counter, x, y): "MP" on the first line, "DOOM" on the second.
_LOGO = [(_M, None, 0, 0), (_P[0], _P[1], 108, 0),
         (_D[0], _D[1], 0, 118), (_O[0], _O[1], 92, 118), (_O[0], _O[1], 184, 118), (_M, None, 276, 118)]
LOGO_SCALE, LOGO_TOP, LOGO_UNITS_WIDE = 0.5, 42, 376


def _fill(mask, points, value):
    """Set pixels whose centres fall inside the polygon (even-odd scanline)."""
    ys = [y for _, y in points]
    for row in range(max(0, int(min(ys))), min(HEIGHT, int(max(ys)) + 1)):
        cy = row + 0.5
        xs = []
        for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1]):
            if (y1 <= cy < y2) or (y2 <= cy < y1):
                xs.append(x1 + (cy - y1) * (x2 - x1) / (y2 - y1))
        xs.sort()
        for a, b in zip(xs[::2], xs[1::2]):
            for col in range(max(0, math.ceil(a - 0.5)), min(WIDTH, math.ceil(b - 0.5))):
                mask[row * WIDTH + col] = value


def _shift(mask, dx, dy):
    out = [False] * (WIDTH * HEIGHT)
    for y in range(max(0, dy), min(HEIGHT, HEIGHT + dy)):
        src = (y - dy) * WIDTH
        for x in range(max(0, dx), min(WIDTH, WIDTH + dx)):
            out[y * WIDTH + x] = mask[src + x - dx]
    return out


def _square(mask, radius, keep):
    """Erode (keep=all) or dilate (keep=any) with a (2r+1)-pixel square, done in two passes."""
    rows = [False] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        base = y * WIDTH
        for x in range(WIDTH):
            rows[base + x] = keep(mask[base + max(0, x - radius):base + min(WIDTH, x + radius + 1)])
    out = [False] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            out[y * WIDTH + x] = keep(rows[yy * WIDTH + x] for yy in range(max(0, y - radius), min(HEIGHT, y + radius + 1)))
    return out


def _mix(c1, c2, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(c1, c2))


def draw_logo(image):
    k, top = LOGO_SCALE, LOGO_TOP
    left = round(WIDTH / 2 - LOGO_UNITS_WIDE * k / 2)
    letters = [False] * (WIDTH * HEIGHT)
    for outline, counter, ux, uy in _LOGO:
        _fill(letters, [(left + (ux + a) * k, top + (uy + b) * k) for a, b in outline], True)
        if counter:
            _fill(letters, [(left + (ux + a) * k, top + (uy + b) * k) for a, b in counter], False)
    line_h, line_tops = 100 * k, [top, top + 118 * k]

    def line_t(y):
        line_top = min(line_tops, key=lambda t: abs(t + line_h / 2 - y))
        return max(0.0, min(1.0, (y - line_top) / line_h)), line_top

    extrude = [False] * (WIDTH * HEIGHT)
    for dy in range(1, 6):
        extrude = [a or b for a, b in zip(extrude, _shift(letters, 0, dy))]
    outline = _square([a or b for a, b in zip(letters, extrude)], 1, any)
    inner_ring, face = _square(letters, 2, all), _square(letters, 3, all)
    lit, shade = _shift(letters, 1, 1), _shift(letters, -1, -1)
    rim = [(0.0, (255, 243, 205)), (0.5, (255, 182, 110)), (1.0, (222, 88, 42))]

    def put(i, rgb):
        image[i] = rgb[0] << 16 | rgb[1] << 8 | rgb[2]

    for i in range(WIDTH * HEIGHT):
        y, x = divmod(i, WIDTH)
        if letters[i]:
            continue
        if extrude[i]:
            put(i, _mix((150, 52, 20), (52, 10, 4), line_t(y - 3)[0]))
        elif outline[i]:
            put(i, (12, 3, 2))
    for i in range(WIDTH * HEIGHT):
        if not letters[i]:
            continue
        y, x = divmod(i, WIDTH)
        t, line_top = line_t(y)
        if face[i]:
            # Tech metal above a drum-hit waveform, gold bricks below it.
            u, local = (x - left) / k, (y - line_top) / k
            beat_x, beat = u % 47, int(u // 47)
            amp = (17 if beat % 2 == 0 else 12) * math.exp(-beat_x / 13) + 1.5
            wave = 48 + amp * math.sin(2 * math.pi * beat_x / 9.5)
            gx, gy = (x - left) % 8, int(y - line_top) % 8
            if abs(local - wave) * k < 0.6:
                rgb = (255, 226, 122)
            elif local < wave:
                rgb = (30, 58, 74) if gx == 0 or gy == 3 else (63, 110, 134)
                if (gx, gy) in ((5, 1), (2, 6)):
                    rgb = (191, 234, 245)
                elif (gx, gy) in ((3, 1), (6, 5)):
                    rgb = (120, 178, 200)
                rgb = _mix(rgb, (255, 255, 255), 0.18 - t * 0.3)
            else:
                ry = int(y - line_top)
                bx = (x - left + (3 if (ry // 3) % 2 else 0)) % 6
                rgb = (165, 80, 15) if ry % 3 == 0 or bx == 0 else (255, 212, 126) if ry % 3 == 1 else (240, 165, 49)
                rgb = _mix(rgb, (110, 24, 8), (t - 0.55) * 1.1)
        elif inner_ring[i]:
            rgb = (90, 20, 8)
        else:
            stop = 0 if t <= 0.5 else 1
            rgb = _mix(rim[stop][1], rim[stop + 1][1], (t - rim[stop][0]) / 0.5)
        if not lit[i]:
            rgb = (255, 248, 224)
        elif not shade[i] and not inner_ring[i]:
            rgb = (138, 42, 12)
        put(i, rgb)


def generate(build):
    path = Path(build) / 'mpc_opening_pixels.inc'
    data = pixels()
    with path.open('w', newline='\n') as out:
        out.write('/* Generated by doom_opening_sources.py; original 320x240 XRGB artwork. */\n')
        out.write('static const uint32_t mpc_opening_pixels[320*240] = {\n')
        for start in range(0, len(data), 8):
            out.write('    ' + ', '.join(f'0x{value:08x}u' for value in data[start:start+8]) + ',\n')
        out.write('};\n')
    return path
