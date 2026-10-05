"""Rebuild only the terrain material, then freshly validate all saved v0.4 assets."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
for name in ('import_terrain_v04.py','verify_environment_v04.py'):
    path=ROOT/'Tools'/name
    exec(compile(path.read_text(),str(path),'exec'),{'__file__':str(path),'__name__':'__main__'})
