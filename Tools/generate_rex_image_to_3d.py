"""Generate Rex base meshes from the owner's photographs with hosted image-to-3D models.

Runs on the development PC with the bundled Blender Python (gradio_client installed).
Tries public Hugging Face Spaces in order; every endpoint that accepts an image is
attempted and any .glb/.obj/.ply it returns is saved to
Art/CompanionDog/Source/generated/. Logs to Saved/claude-image3d.log.
Optional: a Hugging Face token in Saved/hf_token.txt raises the free GPU quota.
"""
import json, os, shutil, sys, time, traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Art/CompanionDog/Source/generated'
OUT.mkdir(parents=True, exist_ok=True)
LOG = ROOT / 'Saved/claude-image3d.log'
PHOTOS = ROOT / 'Saved/rex_photos'
MAIN = PHOTOS / 'rex_main_crop.png'
TOKEN = None
tok = ROOT / 'Saved/hf_token.txt'
if tok.exists():
    TOKEN = tok.read_text(encoding='utf-8').strip() or None


def log(*parts):
    line = time.strftime('%H:%M:%S ') + ' '.join(str(p) for p in parts)
    print(line, flush=True)
    with open(LOG, 'a', encoding='utf-8') as f:
        f.write(line + '\n')


try:
    from gradio_client import Client, handle_file
except Exception as e:
    log('IMPORT_FAIL gradio_client', e); sys.exit(2)

SAVED = []


def collect_files(result, tag):
    saved = []
    def walk(x):
        if isinstance(x, str):
            if x.lower().endswith(('.glb', '.obj', '.ply', '.zip')) and os.path.exists(x) and os.path.getsize(x) > 2000:
                dest = OUT / f'{tag}_{len(saved)}{Path(x).suffix.lower()}'
                shutil.copyfile(x, dest); saved.append(dest); log('SAVED', dest, os.path.getsize(dest))
        elif isinstance(x, dict):
            for v in x.values(): walk(v)
        elif isinstance(x, (list, tuple)):
            for v in x: walk(v)
    walk(result)
    SAVED.extend(saved)
    return saved


def params(info):
    return [(p.get('parameter_name') or '', (p.get('python_type') or {}).get('type', ''), p.get('parameter_has_default', False), p.get('parameter_default')) for p in info.get('parameters', [])]


def kwargs_for(info, image):
    kw = {}
    for name, ptype, has_default, default in params(info):
        lname = name.lower()
        if lname.startswith('mv_image') or 'multiimage' in lname or lname in ('image_prompts',):
            continue
        if ('image' in lname or lname in ('input_image', 'img', 'file')) and 'remove' not in lname and 'bg' not in lname and 'filepath' in ptype:
            kw[name] = handle_file(str(image)); continue
        if lname in ('caption', 'prompt', 'text'):
            kw[name] = ''; continue
        if lname == 'seed':
            kw[name] = 1234; continue
        if 'randomize' in lname:
            kw[name] = False; continue
        if 'octree' in lname:
            kw[name] = 320; continue
        if 'rembg' in lname or 'remove_bg' in lname or 'background' in lname or 'foreground' in lname:
            kw[name] = True; continue
        if 'num_chunks' in lname:
            kw[name] = 8000; continue
        if 'texture_size' in lname:
            kw[name] = 2048; continue
        if 'simplify' in lname:
            kw[name] = 0.9; continue
        if 'Literal' in ptype and lname.endswith('mode') and 'Standard' in ptype:
            kw[name] = 'Standard'; continue
        if not has_default:
            log('WARN no value for required parameter', name, ptype)
    return kw


def run_space(space, tag, wanted=None):
    client = Client(space, verbose=False, **({'token': TOKEN} if TOKEN else {}))
    api = client.view_api(return_format='dict', print_info=False)
    eps = api.get('named_endpoints', {})
    for name, info in eps.items():
        if name.startswith('/lambda'):
            continue
        log('ENDPOINT', name, json.dumps(params(info))[:900])
    order = []
    for key in ('generation_all', 'generate', 'run', 'image_to_3d', 'shape_generation', 'shape', 'predict', 'process', 'infer'):
        for name in eps:
            if key in name.lower() and name not in order:
                order.append(name)
    if wanted:
        order = [n for n in wanted if n in eps] + [n for n in order if n not in wanted]
    # Multi-step spaces (TRELLIS-like): preprocess first so the session holds the state.
    for name in order:
        info = eps[name]
        if not any('filepath' in t and 'image' in n.lower() for n, t, _, _ in params(info)):
            continue
        try:
            if '/preprocess_image' in eps and name == '/image_to_3d':
                client.predict(image=handle_file(str(MAIN)), api_name='/preprocess_image')
                log('PREPROCESSED')
            kw = kwargs_for(info, MAIN)
            log('CALL', space, name, json.dumps({k: (v if not isinstance(v, dict) else 'file') for k, v in kw.items()}))
            result = client.predict(api_name=name, **kw)
            log('RESULT', name, str(result)[:600])
            files = collect_files(result, tag + name.strip('/').replace('/', '_'))
            if name == '/image_to_3d' and '/extract_glb' in eps:
                kw2 = kwargs_for(eps['/extract_glb'], MAIN)
                result2 = client.predict(api_name='/extract_glb', **kw2)
                log('RESULT /extract_glb', str(result2)[:400])
                files += collect_files(result2, tag + 'extract_glb')
            if files:
                return files
        except Exception as e:
            log('FAIL', space, name, repr(e)[:500])
    return []


attempts = [
    ('tencent/Hunyuan3D-2.1', 'hy21', ['/shape_generation']),
    ('VAST-AI/TripoSG', 'triposg', None),
    ('stabilityai/stable-fast-3d', 'sf3d', None),
    ('TencentARC/InstantMesh', 'instantmesh', None),
    ('Stable-X/Hi3DGen', 'hi3dgen', None),
    ('trellis-community/TRELLIS', 'trellis', None),
    ('tencent/Hunyuan3D-2', 'hy20', ['/shape_generation']),
]
only = [a for a in sys.argv[1:] if not a.startswith('-')]
for space, tag, wanted in attempts:
    if only and tag not in only:
        continue
    log('SPACE', space)
    try:
        files = run_space(space, tag, wanted)
        if files and '--all' not in sys.argv:
            break
    except Exception as e:
        log('FAIL', space, repr(e)[:500])
log('DONE', len(SAVED), [str(f) for f in SAVED])
sys.exit(0 if SAVED else 1)
