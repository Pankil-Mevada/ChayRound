"""Offline, non-destructive SQLite backup/restore. Never overwrite a data directory."""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import sqlite3

DATABASES=('merchant.db','order.db')

def integrity(file):
    with sqlite3.connect(f'file:{file}?mode=ro',uri=True) as db:
        if db.execute('PRAGMA integrity_check').fetchone()[0]!='ok':
            raise RuntimeError(f'Integrity check failed: {file.name}')
        return db.execute('PRAGMA user_version').fetchone()[0]

def backup(data,output):
    data=data.resolve()
    if output.exists():
        raise RuntimeError('Backup output must not already exist')
    with open(data/'stack.lock','a') as lock:
        try: fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as error: raise RuntimeError('Stop scripts/dev.py before backing up both services') from error
        for name in DATABASES:
            if not (data/name).is_file(): raise RuntimeError(f'Missing {name}')
        output.mkdir(parents=True)
        output.chmod(0o700)
        manifest={}
        for name in DATABASES:
            target=output/name
            with sqlite3.connect(f'file:{data/name}?mode=ro',uri=True) as src,sqlite3.connect(target) as dst:
                src.backup(dst)
            target.chmod(0o600)
            manifest[name]={'schema':integrity(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
        (output/'manifest.json').write_text(json.dumps(manifest,indent=2))
        print('Both service databases backed up and integrity checked. Protect this directory as sensitive data.')

def restore(source,data):
    manifest=json.loads((source/'manifest.json').read_text())
    if data.exists(): raise RuntimeError('Restore target must be a new directory; existing data is never replaced')
    for name in DATABASES:
        file=source/name
        if hashlib.sha256(file.read_bytes()).hexdigest()!=manifest[name]['sha256']:
            raise RuntimeError('Backup digest mismatch')
        if integrity(file)!=manifest[name]['schema']: raise RuntimeError('Schema mismatch')
    data.mkdir(parents=True)
    data.chmod(0o700)
    for name in DATABASES:
        with sqlite3.connect(f'file:{source/name}?mode=ro',uri=True) as src,sqlite3.connect(data/name) as dst:
            src.backup(dst)
        (data/name).chmod(0o600)
    print('Restored to a new directory; original data unchanged. Pending verification codes require a new request.')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('operation',choices=['backup','restore'])
    parser.add_argument('--data-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--source',type=Path)
    args=parser.parse_args()
    if args.operation=='backup':
        if not args.output: parser.error('--output is required')
        backup(args.data_dir,args.output)
    else:
        if not args.source: parser.error('--source is required')
        restore(args.source,args.data_dir)
