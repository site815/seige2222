"""Rebuild grass/ground materials only, then validate saved asset contracts."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
for name in ('calibrate_meadow_materials.py','import_terrain_v04.py','verify_environment_v04.py'):
    script=ROOT/'Tools'/name
    exec(compile(script.read_text(),str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
