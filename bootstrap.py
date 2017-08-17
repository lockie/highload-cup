#!/usr/bin/env python3

import glob
import json
import os
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
import tempfile
import time
import zipfile

from models import Base


def get_class_by_tablename(tablename):
    for c in Base._decl_class_registry.values():
        if hasattr(c, '__tablename__') and c.__tablename__ == tablename:
            return c
    return None


def import_json_file(filename, session):
    with open(filename, 'r') as f:
        data = json.loads(f.read())
        kind = list(data.keys())[0]
        klass = get_class_by_tablename(kind)
        if klass:
            print('Processing ' + kind + ' file `' + filename + '`')
            for obj in data[kind]:
                session.add(klass(**obj))
            session.commit()


def main():
    start = time.time()
    engine = create_engine('postgresql+psycopg2://root@/default')
    Base.metadata.create_all(engine)

    Session = sessionmaker()
    Session.configure(bind=engine)
    with tempfile.TemporaryDirectory() as directory:
        data = zipfile.ZipFile('/tmp/data/data.zip', 'r')
        data.extractall(directory)
        data.close()
        for filename in sorted(glob.glob(directory + '/*.json'),
                               key=lambda f: 2 if 'visits' in f else 1):
            # XXX is it okay to have one transaction per file?..
            import_json_file(filename, Session())
            os.remove(filename)
    engine.dispose()
    print('Processing took ' + str(time.time() - start) + 's')


if __name__ == '__main__':
    main()
