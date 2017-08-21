#!/usr/bin/env python3

import sys


def main(argv):
    if len(argv) != 3:
        print('Usage: ' + argv[0] + ' <ammo file> <urls.txt file>')
        return 1

    with open(argv[1], 'rb') as ammofile:
        ammo = ammofile.read().decode('utf8')

    with open(argv[2], 'a') as urls:
        prevline = None
        post = None
        for line in ammo.split('\n'):
            if line.startswith('GET'):
                urls.write(
                    'http://localhost' + line.split()[1] + '\n')
            if line.startswith('POST'):
                post = line.split()[1]
            if prevline == '\r' and post:
                urls.write(
                    'http://localhost' + post + ' POST ' + line + '\n')
                post = None
            prevline = line
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
