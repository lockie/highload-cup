#!/usr/bin/env python3

import json
from os.path import splitext
import sys
import subprocess
import tempfile
import time
import zipfile


def main():
    start = time.time()

    with tempfile.NamedTemporaryFile(mode='w', delete=False) as sql:
        sql.write('''
            CREATE TABLE locations (
                id integer NOT NULL,
                place character varying,
                country character varying(50),
                city character varying(50),
                distance integer
            );
            CREATE TABLE users (
                id integer NOT NULL,
                email character varying(100),
                first_name character varying(50),
                last_name character varying(50),
                gender character(1),
                birth_date integer
            );
            CREATE TABLE visits (
                id integer NOT NULL,
                location integer,
                "user" integer,
                visited_at integer,
                mark smallint
            );
            ''')
        with zipfile.ZipFile('/tmp/data/data.zip', 'r') as data:
            for filename in data.namelist():
                kind = splitext(filename)[0].split('_')[0]
                rows = json.loads(data.read(filename))
                # XXX assuming key order is the same throughout the file
                columns = ','.join(rows[kind][0].keys())
                if kind == 'visits':
                    # сраный Postgres с его сраным синтаксисом
                    columns = columns.replace('user', '"user"')
                sql.write('COPY ' + kind + '(' +
                          columns + ') FROM STDIN (FORMAT csv);\n')
                stdin = ""
                for row in rows[kind]:
                    stdin += ','.join(map(str, row.values())) + '\n'
                sql.write(stdin + '\\.\n\n')

        sql.write('''
            CREATE SEQUENCE locations_id_seq
                INCREMENT BY 1
                NO MINVALUE
                NO MAXVALUE
                CACHE 1;
            SELECT setval('locations_id_seq', (SELECT MAX(id) FROM locations));
            ALTER TABLE ONLY locations ALTER COLUMN id SET DEFAULT
                    nextval('locations_id_seq'::regclass);

            CREATE SEQUENCE users_id_seq
                INCREMENT BY 1
                NO MINVALUE
                NO MAXVALUE
                CACHE 1;
            SELECT setval('users_id_seq', (SELECT MAX(id) FROM users));
            ALTER TABLE ONLY users ALTER COLUMN id SET DEFAULT
                    nextval('users_id_seq'::regclass);

            CREATE SEQUENCE visits_id_seq
                INCREMENT BY 1
                NO MINVALUE
                NO MAXVALUE
                CACHE 1;
            SELECT setval('visits_id_seq', (SELECT MAX(id) FROM visits));
            ALTER TABLE ONLY visits ALTER COLUMN id SET DEFAULT
                    nextval('visits_id_seq'::regclass);

            ALTER TABLE ONLY locations
                ADD CONSTRAINT locations_pkey PRIMARY KEY (id);

            ALTER TABLE ONLY users
                ADD CONSTRAINT users_pkey PRIMARY KEY (id);

            ALTER TABLE ONLY visits
                ADD CONSTRAINT visits_pkey PRIMARY KEY (id);

            ALTER TABLE ONLY visits
                ADD CONSTRAINT visits_location_fkey FOREIGN KEY (location)
                REFERENCES locations(id);

            ALTER TABLE ONLY visits
                ADD CONSTRAINT visits_user_fkey FOREIGN KEY ("user")
                REFERENCES users(id);

            ANALYZE;
            ''')

    res = subprocess.call(['/usr/src/app/wait-for-postgres.sh', 'psql',
                            '-q', '-d', 'default', '-1', '-f', sql.name])
    if res != 0:
        return res

    print('Bootstrapping took ' + str(time.time() - start) + 's')
    return 0


if __name__ == '__main__':
    sys.exit(main())
