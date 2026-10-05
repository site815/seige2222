"""Acquire explicitly selected CC0 environment assets; development only.

Powered by Poly Haven (https://polyhaven.com) and ambientCG.
The packaged game never contacts either provider.
"""
from pathlib import Path
import urllib.request,json,hashlib,concurrent.futures,zipfile
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04';CACHE=ROOT/'.tools/nature-downloads'
ART.mkdir(parents=True,exist_ok=True)
HEADERS={'User-Agent':'SEIGE local environment preparation - Powered by Poly Haven'}
def get_json(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers=HEADERS),timeout=90) as r:return json.load(r)
def fetch(path,record):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.stat().st_size!=record['size']:
        with urllib.request.urlopen(urllib.request.Request(record['url'],headers=HEADERS),timeout=180) as r,path.open('wb') as out:
            while block:=r.read(1024*1024):out.write(block)
    md5=hashlib.md5();sha=hashlib.sha256()
    with path.open('rb') as data:
        while block:=data.read(1024*1024):md5.update(block);sha.update(block)
    if path.stat().st_size!=record['size'] or ('md5' in record and record['md5']!=md5.hexdigest()):raise RuntimeError('Source verification failed '+str(path))
    return {'local_path':path.relative_to(ROOT).as_posix(),'url':record['url'],'bytes':path.stat().st_size,'sha256':sha.hexdigest(),'official_md5':record.get('md5')}
manifest={'license':'CC0-1.0','license_urls':['https://polyhaven.com/license','https://docs.ambientcg.com/license/'],'assets':{}}
jobs=[]
for asset in ('jacaranda_tree','grass_bermuda_01','grass_medium_02','forest_leaves_02'):
    files=json.loads((ROOT/'.tools/nature-catalog'/(asset+'_files.json')).read_text(encoding='utf-8-sig'))
    info=get_json('https://api.polyhaven.com/info/'+asset)
    entry={'provider':'Poly Haven','source_page':'https://polyhaven.com/a/'+asset,'authors':info.get('authors',{}),'maps':{}}
    manifest['assets'][asset]=entry
    if asset!='forest_leaves_02':
        original=files['blend']['1k']['blend'];jobs.append((asset,'source',CACHE/asset/(asset+'_1k.blend'),original))
    for key,variants in files.items():
        key=key.lower();role=None
        if key.startswith('dry_'):continue
        for suffix,channel in (('diffuse','color'),('diff','color'),('nor_gl','normal'),('rough','roughness'),('alpha','alpha'),('ao','ao')):
            if key==suffix or key.endswith('_'+suffix):role=channel;group=key[:-len(suffix)].rstrip('_') or 'surface';break
        if role is None:continue
        resolution='4k' if asset=='jacaranda_tree' and group=='leaves' and role in ('color','alpha') else ('1k' if role in ('roughness','ao') else '2k')
        formats=variants[resolution];extension='png' if role=='alpha' else 'jpg';record=formats.get(extension) or next(iter(formats.values()))
        jobs.append((asset,(group,role),ART/'Textures'/asset/Path(record['url']).name,record))
ambient=json.loads((ROOT/'.tools/nature-catalog/ambient-grass004.json').read_text(encoding='utf-8-sig'))['foundAssets'][0]
bundle=next(v for v in ambient['downloadFolders']['default']['downloadFiletypeCategories']['zip']['downloads'] if v['attribute']=='2K-JPG')
jobs.append(('Grass004','bundle',CACHE/'Grass004'/bundle['fileName'],{'url':bundle['downloadLink'],'size':bundle['size']}))
manifest['assets']['Grass004']={'provider':'ambientCG','source_page':'https://ambientcg.com/a/Grass004','authors':{'Lennart Demes':'ambientCG'},'physical_size_cm':[140,140],'maps':{},'creation_method':'PBRProcedural, as listed by provider'}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    work={pool.submit(fetch,path,record):(asset,slot) for asset,slot,path,record in jobs}
    for job in concurrent.futures.as_completed(work):
        asset,slot=work[job];record=job.result()
        if slot in ('source','bundle'):manifest['assets'][asset]['original_'+slot]=record
        else:
            group,role=slot;manifest['assets'][asset]['maps'].setdefault(group,{})[role]=record
        print('VERIFIED',record['local_path'],flush=True)
entry=manifest['assets']['Grass004'];path=ROOT/entry['original_bundle']['local_path']
with zipfile.ZipFile(path) as archive:
    for filename in archive.namelist():
        roles={'Color':'color','NormalGL':'normal','Roughness':'roughness','AmbientOcclusion':'ao','Displacement':'height'}
        role=next((v for k,v in roles.items() if filename.endswith('_'+k+'.jpg')),None)
        if not role:continue
        target=ART/'Textures/Grass004'/Path(filename).name;target.parent.mkdir(parents=True,exist_ok=True);data=archive.read(filename);target.write_bytes(data)
        entry['maps'].setdefault('surface',{})[role]={'local_path':target.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'archive_member':filename,'url':entry['original_bundle']['url']}
(ART/'sources.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('ENVIRONMENT_V04_DOWNLOAD_COMPLETE',flush=True)
