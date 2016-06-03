#!/usr/bin/env python

import os
import subprocess
import sys

from lib.config import PLATFORM, get_target_arch, get_platform_key, s3_config
from lib.util import electron_gyp, get_electron_version, s3put


ATOM_SHELL_REPO = 'electron/electron'
ATOM_SHELL_VERSION = get_electron_version()

PROJECT_NAME = electron_gyp()['project_name%']
PRODUCT_NAME = electron_gyp()['product_name%']

SOURCE_ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
OUT_DIR = os.path.join(SOURCE_ROOT, 'out', 'R')
DIST_DIR = os.path.join(SOURCE_ROOT, 'dist')
DIST_NAME = '{0}-{1}-{2}-{3}.zip'.format(PROJECT_NAME,
                                         ATOM_SHELL_VERSION,
                                         get_platform_key(),
                                         get_target_arch())
SYMBOLS_NAME = '{0}-{1}-{2}-{3}-symbols.zip'.format(PROJECT_NAME,
                                                    ATOM_SHELL_VERSION,
                                                    get_platform_key(),
                                                    get_target_arch())
DSYM_NAME = '{0}-{1}-{2}-{3}-dsym.zip'.format(PROJECT_NAME,
                                              ATOM_SHELL_VERSION,
                                              get_platform_key(),
                                              get_target_arch())


def main():
    commit = subprocess.check_output(['git', 'show', '--format=%h', '-s',
                                      '--abbrev=12', 'HEAD']).strip()
    target = 'mozilla/tofino-electron/{0}'.format(commit)

    upload(target, os.path.join(DIST_DIR, DIST_NAME))
    upload(target, os.path.join(DIST_DIR, SYMBOLS_NAME))
    if PLATFORM == 'darwin':
        upload(target, os.path.join(DIST_DIR, DSYM_NAME))


def upload(path, filename):
    (bucket, access_key, secret_key) = s3_config()

    s3put(bucket, access_key, secret_key, os.path.dirname(filename), path,
          [filename])


if __name__ == '__main__':
    sys.exit(main())
