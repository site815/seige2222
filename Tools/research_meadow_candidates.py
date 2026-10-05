"""Stage official CC0 source candidates in the ignored research cache only.

Powered by Poly Haven. No Unreal assets or current sources are replaced.
"""
from pathlib import Path
import urllib.request,json,hashlib,concurrent.futures
ROOT=Path(__file__).resolve().parents[1];CACHE=ROOT/'.tools/meadow-research'
CACHE.mkdir(parents=True,exist_ok=True)
HEADERS={'User-Agent':'SEIGE local asset research - Powered by Poly Haven'}
def read(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers=HEADERS),timeout=120) as r:return r.read()
def get(record,path):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists():path.write_bytes(read(record['url']))
    b=path.read_bytes()
    if len(b)!=record['size'] or hashlib.md5(b).hexdigest()!=record['md5']:raise RuntimeError('Invalid official source '+str(path))
    return {'path':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(b).hexdigest(),'url':record['url']}
result={}
for name in ('grass_medium_01','grass_medium_02'):
    data=json.loads(read('https://api.polyhaven.com/files/'+name));info=json.loads(read('https://api.polyhaven.com/info/'+name))
    (CACHE/(name+'_files.json')).write_text(json.dumps(data,indent=2))
    original=data['blend']['1k']['blend'];out=CACHE/name
    jobs=[(original,out/(name+'.blend'))]
    for file,r in original.get('include',{}).items():jobs.append((r,out/file))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:records=list(pool.map(lambda v:get(*v),jobs))
    result[name]={'license':'CC0-1.0','source_page':'https://polyhaven.com/a/'+name,'authors':info['authors'],'files':records}
    print('STAGED',name,flush=True)
(CACHE/'sources.json').write_text(json.dumps(result,indent=2))
print('MEADOW_CANDIDATES_STAGED',flush=True)
