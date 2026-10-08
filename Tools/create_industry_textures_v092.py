"""Authored tileable PBR surfaces for the industry palette (v0.9.2 art pass 3).

Replaces the near-uniform v0.8 maps in Art/Textures/Industry with panelled, bolted,
weathered surfaces so MI_Industry_* carries visible material detail at the player
zoom. The master material multiplies Color by the palette tint, so every map stays
bright (mean ~0.84) and only *varies* brightness; the palette keeps its identity.

Tile = 160 cm (the kit's box-projected UV scale), 1024 px, so 1 cm = 6.4 px.
Every feature wraps, so the maps tile without seams.

    python3 Tools/create_industry_textures_v092.py            # writes the nine PNGs
    python3 Tools/create_industry_textures_v092.py --preview  # also a contact sheet

numpy + scipy + Pillow only (no Blender). Afterwards run import_industry_assets.py
with -IndustryMaterialsOnly on the PC to reimport the textures and rebuild the
material instances; the kit meshes share those instances and update automatically.
"""
import json, sys
from pathlib import Path
import numpy as np
from scipy import ndimage
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]; ART = ROOT / 'Art'; TEX = ART / 'Textures/Industry'
TEX.mkdir(parents=True, exist_ok=True)
N = 1024                     # pixels per 160 cm tile
PX = N / 160.0               # pixels per centimetre
rng = np.random.default_rng(92)
yy, xx = np.mgrid[0:N, 0:N]


def tile_noise(cells, seed=None, order=3):
    """Periodic smooth value noise with `cells` lattice points per tile."""
    r = np.random.default_rng(seed) if seed is not None else rng
    grid = r.random((cells, cells)).astype(np.float32)
    up = ndimage.zoom(grid, N / cells, order=order, mode='grid-wrap', grid_mode=True)[:N, :N]
    if up.shape != (N, N):
        up = np.kron(grid, np.ones((N // cells + 1, N // cells + 1), np.float32))[:N, :N]
    return ndimage.gaussian_filter(up, sigma=N / cells * .25, mode='wrap')


def fbm(octaves=(4, 8, 16, 32, 64), gain=.55):
    acc = np.zeros((N, N), np.float32); amp = 1.; total = 0.
    for cells in octaves:
        acc += amp * tile_noise(cells); total += amp; amp *= gain
    acc /= total
    return (acc - acc.min()) / (acc.max() - acc.min() + 1e-9)


def white():
    return rng.random((N, N)).astype(np.float32)


def normal_from_height(height, strength=1.0):
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    length = np.sqrt(dx * dx + dy * dy + 1)
    # +Y up (OpenGL); the importer flips the green channel for Unreal.
    return np.stack((-dx / length, dy / length, 1 / length), axis=2) * .5 + .5


def grooves(positions_cm, width_cm, axis, depth=1.0, profile='v'):
    """Periodic straight seams across the whole tile at the given centimetre offsets."""
    coord = (xx if axis == 'x' else yy) / PX
    out = np.zeros((N, N), np.float32); half = width_cm / 2
    for p in positions_cm:
        d = np.abs(((coord - p + 80) % 160) - 80)
        if profile == 'v':
            out = np.maximum(out, np.clip(1 - d / half, 0, 1))
        else:
            out = np.maximum(out, (d <= half).astype(np.float32))
    return out * depth


def dots(centres_cm, radius_cm, profile='dome'):
    """Periodic round features (rivets, tie holes) at centimetre coordinates."""
    out = np.zeros((N, N), np.float32)
    for cx, cy in centres_cm:
        dx = ((xx / PX - cx + 80) % 160) - 80; dy = ((yy / PX - cy + 80) % 160) - 80
        d = np.sqrt(dx * dx + dy * dy) / radius_cm
        if profile == 'dome':
            out = np.maximum(out, np.sqrt(np.clip(1 - d * d, 0, 1)))
        else:
            out = np.maximum(out, np.clip(1 - d, 0, 1))
    return out


def streaks(seed_mask, length_px, decay=None):
    """Downward (+y in texture space = down the wall) weathering streaks from a seed mask."""
    decay = decay if decay is not None else 1 - 1 / length_px
    out = np.zeros_like(seed_mask); carry = np.zeros(N, np.float32)
    # Two passes so the wrap seam carries properly.
    for _ in range(2):
        for row in range(N):
            carry = np.maximum(carry * decay, seed_mask[row]); out[row] = carry
    jitter = tile_noise(64) * .6 + tile_noise(128) * .4
    return out * (.55 + .45 * jitter)


def random_lines(count, length_range, thickness_px, seed):
    """Sparse scratches/cracks drawn on the torus so they tile."""
    r = np.random.default_rng(seed); canvas = np.zeros((N, N), np.float32)
    for _ in range(count):
        x, y = r.random(2) * N; angle = r.random() * np.pi * 2; length = r.uniform(*length_range)
        steps = int(length); wobble = r.uniform(.004, .02)
        for s in range(steps):
            angle += r.normal(0, wobble)
            x += np.cos(angle); y += np.sin(angle)
            canvas[int(y) % N, int(x) % N] = 1
    if thickness_px > 1:
        canvas = ndimage.maximum_filter(canvas, size=int(thickness_px), mode='wrap')
    return ndimage.gaussian_filter(canvas, sigma=.6, mode='wrap')


def save(name, array):
    arr = np.clip(array, 0, 1)
    if arr.ndim == 2: arr = np.repeat(arr[:, :, None], 3, 2)
    Image.fromarray((arr * 255 + .5).astype(np.uint8), 'RGB').save(TEX / (name + '.png'), optimize=True)
    return arr


records = {}


# --------------------------------------------------------------------- PAINT: painted steel cladding
def paint():
    panel = 80.0                                   # two panels per tile each way
    seam = grooves((0, panel), 1.2, 'x', profile='v') + grooves((0, panel), 1.2, 'y', profile='v')
    seam = np.clip(seam, 0, 1)
    # Per-panel tone: each 80 cm panel gets its own slight batch colour.
    panel_id = ((xx // (N // 2)) + 2 * (yy // (N // 2))).astype(int)
    batch = np.array([0.0, .035, -.03, .015], np.float32)[panel_id]
    bolts = dots([(x, y) for x in range(10, 160, 20) for y in (3.2, 76.8, 83.2, 156.8)] +
                 [(x, y) for y in range(10, 160, 20) for x in (3.2, 76.8, 83.2, 156.8)], 1.0)
    bolt_ring = dots([(x, y) for x in range(10, 160, 20) for y in (3.2, 76.8, 83.2, 156.8)] +
                     [(x, y) for y in range(10, 160, 20) for x in (3.2, 76.8, 83.2, 156.8)], 1.5, 'cone')
    broad = fbm((2, 4, 8, 16)); fine = fbm((64, 128)); grain = white()
    grime = fbm((3, 6, 12, 24)) ** 1.6
    drip = streaks(np.clip(bolts * 1.2 + seam * (yy % (N // 2) < 12), 0, 1) * (.5 + .5 * tile_noise(32)), N * .18)
    scratch = random_lines(26, (40, 260), 1.6, 11)
    # Colour: bright base, panel batches, dulling grime in grooves and drips.
    shade = .86 + batch + .05 * (broad - .5) + .025 * (fine - .5) + .012 * (grain - .5)
    shade *= 1 - .22 * seam - .16 * grime * (.6 + .4 * broad) - .14 * drip
    shade *= 1 - .10 * bolt_ring * (1 - bolts)             # shadowed washer
    shade += .07 * scratch                                   # scratches show primer/bright metal
    rough = .48 + .08 * (broad - .5) + .05 * (fine - .5) + .32 * grime + .22 * drip + .25 * seam - .16 * scratch
    height = -.9 * seam + .55 * bolts - .15 * bolt_ring * (1 - bolts) + .06 * (fine - .5) + .02 * (grain - .5) - .35 * scratch
    normal = normal_from_height(height, 2.2)
    return shade, rough, normal, 'Painted steel cladding: 80 cm panels with V-seams, washer-bolted edges, batch tone variation, groove grime, drips and scuffs'


# --------------------------------------------------------------------- STEEL: brushed plate with welds
def steel():
    brushed = np.zeros((N, N), np.float32)
    for k in range(6):
        line = rng.random((N, 1)).astype(np.float32)
        brushed += ndimage.gaussian_filter(np.repeat(line, N, 1), sigma=(.8 + k * .6, 0), mode='wrap')
    brushed = (brushed - brushed.min()) / (brushed.max() - brushed.min() + 1e-9)
    wander = tile_noise(8) * 1.2 + tile_noise(32) * .5                 # rows drift so lines are not ruler straight
    brushed = np.take_along_axis(brushed, ((yy + (wander * PX).astype(int)) % N).astype(int), axis=0)
    weld = np.clip(grooves((0,), 2.4, 'y', profile='v') + grooves((80,), 2.4, 'x', profile='v'), 0, 1)
    weld_bead = weld * (.75 + .25 * tile_noise(128))                      # ripple along the bead
    plate_gap = np.clip(grooves((0,), .6, 'y', profile='v') + grooves((80,), .6, 'x', profile='v'), 0, 1)
    bolts = dots([(x, y) for x in (6, 154) for y in range(8, 160, 16)] + [(x, y) for y in (74, 86) for x in range(8, 160, 16)], 1.1)
    broad = fbm((2, 4, 8)); mid = fbm((16, 32)); grain = white()
    oxide = np.clip((fbm((6, 12, 24, 48)) - .70) * 5, 0, 1) * np.clip((fbm((24, 48, 96)) - .35) * 2.5, 0, 1)
    specks = (white() > .9992).astype(np.float32); specks = ndimage.maximum_filter(specks, 3, mode='wrap') * (.5 + .5 * mid)
    oxide = np.clip(oxide + specks * .8, 0, 1)
    drip = streaks(np.clip(oxide * .6 + bolts * .9, 0, 1), N * .12)
    scratch = random_lines(18, (60, 400), 1.2, 23)
    shade = .84 + .06 * (broad - .5) + .05 * (brushed - .5) + .02 * (grain - .5)
    shade *= 1 - .18 * oxide - .10 * drip - .12 * plate_gap
    shade += .09 * weld_bead * (1 - plate_gap) + .05 * scratch
    tint = np.ones((N, N, 3), np.float32)
    tint[:, :, 1] -= .18 * oxide + .06 * drip; tint[:, :, 2] -= .34 * oxide + .12 * drip   # oxidised areas lean warm
    colour = np.repeat(shade[:, :, None], 3, 2) * tint
    rough = .36 + .10 * (broad - .5) + .14 * (brushed - .5) + .42 * oxide + .22 * drip + .18 * weld + .12 * plate_gap - .12 * scratch
    height = .8 * weld_bead - .7 * plate_gap + .5 * bolts + .05 * (brushed - .5) * (1 - weld) + .08 * oxide * mid - .3 * scratch
    normal = normal_from_height(height, 2.0)
    return colour, rough, normal, 'Brushed steel plate: drifting brush lines, weld beads with ripple, bolted plate edges, oxide bloom and run-off'


# --------------------------------------------------------------------- CONCRETE: formed concrete
def concrete():
    broad = fbm((2, 4, 8)); mid = fbm((16, 32, 64)); fine = fbm((128, 256)); grain = white()
    aggregate = np.clip((white() - .975) * 40, 0, 1); aggregate = ndimage.gaussian_filter(aggregate, .8, mode='wrap')
    form = np.clip(grooves((0,), .8, 'x', profile='v') + grooves((0, 80), .8, 'y', profile='v'), 0, 1)
    ties = dots([(x, y) for x in (40, 120) for y in (40, 120)], 1.6, 'cone')
    tie_hole = dots([(x, y) for x in (40, 120) for y in (40, 120)], .9)
    cracks = random_lines(5, (120, 520), 1.4, 37)
    stain = fbm((3, 6, 12)) ** 2.2
    drip = streaks(np.clip(tie_hole * 1.2 + cracks * .4, 0, 1), N * .22)
    shade = .84 + .08 * (broad - .5) + .05 * (mid - .5) + .04 * (fine - .5) + .03 * (grain - .5)
    shade *= 1 - .20 * stain - .14 * drip - .25 * cracks - .12 * form - .18 * tie_hole + .05 * aggregate
    rough = .82 + .06 * (broad - .5) + .06 * (mid - .5) + .10 * stain + .06 * drip - .10 * aggregate + .08 * cracks
    height = .22 * (mid - .5) + .12 * (fine - .5) + .05 * (grain - .5) - .6 * cracks - .5 * form - .8 * tie_hole * (1 - .3 * ties) - .35 * aggregate
    normal = normal_from_height(height, 2.4)
    return shade, rough, normal, 'Board-formed concrete: 80 cm form panels, tie holes, aggregate pits, hairline cracks, run-off stains'


for kind, build in (('Paint', paint), ('Steel', steel), ('Concrete', concrete)):
    colour, rough, normal, description = build()
    maps = {'Color': save('T_Industry_%s_Color' % kind, colour), 'Roughness': save('T_Industry_%s_Roughness' % kind, rough), 'Normal': save('T_Industry_%s_Normal' % kind, normal)}
    stats = {c: {'mean': round(float(m.mean()), 3), 'std': round(float(m.std()), 3)} for c, m in maps.items()}
    records[kind] = {'description': description, 'pixels': N, 'tile_cm': 160, 'stats': stats}
    print('TEXTURE', kind, json.dumps(stats), flush=True)

manifest_path = ART / 'IndustryExports/industry_manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['texture_pass'] = {'version': 3, 'generator': 'Tools/create_industry_textures_v092.py', 'date': '2026-10-09', 'surfaces': records}
for name, entry in manifest['palette'].items():
    # Stronger authored relief now that the maps carry seams and bolts; emissive lamps stay flat.
    entry['normal_strength'] = 0.0 if entry.get('emission', 0) > 0 else 0.85
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')

if '--preview' in sys.argv:
    sheet = Image.new('RGB', (3 * 512, 3 * 512))
    for row, kind in enumerate(('Paint', 'Steel', 'Concrete')):
        for col, channel in enumerate(('Color', 'Roughness', 'Normal')):
            im = Image.open(TEX / ('T_Industry_%s_%s.png' % (kind, channel))).resize((512, 512), Image.LANCZOS)
            sheet.paste(im, (col * 512, row * 512))
    sheet.save(ART / 'Textures/Industry/contact_sheet_v092.jpg', quality=85)
print('TEXTURES_READY', json.dumps({k: v['stats']['Color'] for k, v in records.items()}))
