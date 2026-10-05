"""Verify original maps against download hashes and record prepared derivatives."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04'
sources=json.loads((ART/'sources.json').read_text());checked=0
for source in sources['assets'].values():
    for maps in source['maps'].values():
        for record in maps.values():
            file=ROOT/record['local_path']
            if not file.exists() or hashlib.sha256(file.read_bytes()).hexdigest()!=record['sha256']:raise RuntimeError('Source mismatch '+str(file))
            if file.stat().st_size!=record['bytes']:raise RuntimeError('Source size mismatch '+str(file))
            checked+=1
prepared={}
for folder in ('Source','Exports'):
    for file in sorted((ART/folder).rglob('*')):
        if file.is_file() and file.suffix in ('.blend','.fbx'):
            prepared[file.relative_to(ART).as_posix()]={'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
            if file.suffix=='.blend' and file.stat().st_size>=100*1024*1024:raise RuntimeError('Source blend exceeds 100 MiB '+str(file))
(ART/'prepared_hashes.json').write_text(json.dumps({'source_maps_verified':checked,'prepared':prepared},indent=2))
print('ENVIRONMENT_SOURCE_VERIFICATION_PASS',checked,'original maps;',len(prepared),'prepared files')
