"""Publish a prepared Virgin release. Credentials stay inside this process."""
import argparse, hashlib, json, os, subprocess, urllib.request
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('version')
p.add_argument('archive', type=Path)
p.add_argument('--commit', required=True)
p.add_argument('--notes', type=Path, required=True)
args = p.parse_args()
archive = args.archive.resolve()
checksum = archive.with_suffix(archive.suffix + '.sha256')
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
assert checksum.read_text().split()[0].lower() == digest, 'Checksum mismatch'
env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never')
result = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n', text=True, capture_output=True, check=True, env=env, timeout=20)
credential = dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)
token = credential['password']
base = 'https://api.github.com/repos/parlue/virgin'
def request(url, data=None, content_type='application/json'):
    req = urllib.request.Request(url, data=data, headers={'Authorization': 'Bearer '+token, 'Accept': 'application/vnd.github+json', 'Content-Type': content_type})
    with urllib.request.urlopen(req, timeout=180) as response:
        return json.load(response)
releases = request(base+'/releases')
tag = 'v'+args.version
release = next((r for r in releases if r['tag_name'] == tag), None)
if release is None:
    release = request(base+'/releases', json.dumps({'tag_name':tag, 'target_commitish':args.commit, 'name':'Virgin '+args.version+' Beta', 'body':args.notes.read_text(encoding='utf-8'), 'draft':True, 'prerelease':False}).encode())
assert release['draft'], 'Existing published release: refusing to overwrite'
assert not release['assets'], 'Existing assets: inspect before retrying'
for path in [archive, checksum]:
    request(release['upload_url'].split('{')[0]+'?name='+path.name, path.read_bytes(), 'application/octet-stream')
req = urllib.request.Request(base+'/releases/'+str(release['id']), data=json.dumps({'draft':False, 'make_latest':'true'}).encode(), method='PATCH', headers={'Authorization':'Bearer '+token, 'Content-Type':'application/json'})
with urllib.request.urlopen(req, timeout=30) as response:
    published = json.load(response)
print(published['html_url'])
for asset in published['assets']:
    print(asset['browser_download_url'])
