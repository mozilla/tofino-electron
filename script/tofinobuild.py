#!/usr/bin/env python

import os
import subprocess
import sys
import tempfile
import urlparse
import urllib
import shutil

from lib.config import PLATFORM, BASE_URL, LIBCHROMIUMCONTENT_COMMIT
from lib.util import download, execute, rm_rf, scoped_env, get_host_arch, import_vs_env


SOURCE_ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
PLATFORM_KEY = {
  'cygwin': 'win',
  'darwin': 'osx',
  'linux2': 'linux',
  'win32': 'win',
}[sys.platform]


def path2url(path):
    return urlparse.urljoin(
      'file:', urllib.pathname2url(path))


# Libchromiumcontent is big. Really big. Sometimes you have to retry to get it
# all if your connection isn't perfect.
def retry_download(base_url, filename, target_dir):
    url = "%s/%s" % (base_url, filename)
    target = os.path.join(target_dir, filename)
    args = ["-o", target, "--url", url]

    for i in range(0, 10):
        try:
            subprocess.check_call(["curl"] + args)
            return
        except subprocess.CalledProcessError as e:
            print("Download attempt failed with %d" % (e.returncode))
            if i == 0:
                args += ["-C", "-"]

    raise Exception("Unable to complete download of %s in 10 attempts." % filename)


def main():
  os.chdir(SOURCE_ROOT)

  target_arch = 'x64'
  if os.environ.has_key('TARGET_ARCH'):
    target_arch = os.environ['TARGET_ARCH']
  elif sys.platform == 'win32':
    if os.environ.has_key('Platform'):
      target_arch = os.environ['Platform'].lower()
    else:
      target_arch = 'ia32'

  print("Building for %s." % target_arch)

  if sys.platform == 'win32':
    download('Downloading yasm',
             'http://www.tortall.net/projects/yasm/releases/yasm-1.3.0-win32.exe',
             os.path.join('vendor', 'ffmpeg', 'yasm', 'yasm.exe'))

  LCC_BASE_URL = "%s/%s/%s/%s" % (BASE_URL, PLATFORM_KEY, target_arch, LIBCHROMIUMCONTENT_COMMIT)
  temp_dir = tempfile.mkdtemp()
  target_dir = os.path.join(temp_dir, PLATFORM_KEY, target_arch, LIBCHROMIUMCONTENT_COMMIT)
  os.makedirs(target_dir)

  retry_download(LCC_BASE_URL, "libchromiumcontent.zip", target_dir)
  retry_download(LCC_BASE_URL, "libchromiumcontent-static.zip", target_dir)

  args = ['-y', '--target_arch=' + target_arch, '-u', path2url(temp_dir)]
  run_script('bootstrap.py', args)

  shutil.rmtree(temp_dir)

  build_ffmpeg(target_arch)

  run_script('build.py', ['-c', 'R'])

  run_script('create-dist.py')


def run_script(script, args=[]):
  sys.stderr.write('\nRunning ' + script +'\n')
  sys.stderr.flush()
  script = os.path.join(SOURCE_ROOT, 'script', script)
  subprocess.check_call([sys.executable, script] + args)


def log_versions():
  sys.stderr.write('\nnode --version\n')
  sys.stderr.flush()
  subprocess.call(['node', '--version'])

  sys.stderr.write('\nnpm --version\n')
  sys.stderr.flush()
  npm = 'npm.cmd' if PLATFORM == 'win32' else 'npm'
  subprocess.call([npm, '--version'])


def build_ffmpeg(target_arch):
  env = os.environ.copy()
  if PLATFORM == 'linux' and target_arch != get_host_arch():
    env['GYP_CROSSCOMPILE'] = '1'
  elif PLATFORM == 'win32':
    env['GYP_MSVS_VERSION'] = '2015'
  python = sys.executable
  if sys.platform == 'cygwin':
    # Force using win32 python on cygwin.
    python = os.path.join('vendor', 'python_26', 'python.exe')
  gyp = os.path.join('vendor', 'brightray', 'vendor', 'gyp', 'gyp_main.py')
  gyp_pylib = os.path.join(os.path.dirname(gyp), 'pylib')
  # Avoid using the old gyp lib in system.
  env['PYTHONPATH'] = os.path.pathsep.join([gyp_pylib,
                                            env.get('PYTHONPATH', '')])

  defines = [
    '-Dtarget_arch={0}'.format(target_arch),
    '-Dhost_arch={0}'.format(get_host_arch()),
  ]

  ffmpeg = os.path.join('vendor', 'ffmpeg')
  ffmpeg_gyp = os.path.join(ffmpeg, 'ffmpeg', 'ffmpeg.gyp')
  ffmpeg_config = os.path.join(ffmpeg, 'config.gypi')
  ret = subprocess.call([python, gyp, '-f', 'ninja', '--no-parallel', '--depth', ffmpeg,
                         ffmpeg_gyp, "-I%s" % ffmpeg_config] + defines, env=env)
  if ret != 0:
    sys.exit(ret)

  ninja = os.path.join('vendor', 'depot_tools', 'ninja')
  if sys.platform == 'win32':
    ninja += '.exe'

  build_path = os.path.join(ffmpeg, 'out', 'Default')
  ret = subprocess.call([ninja, '-v', '-C', build_path])
  if ret != 0:
    sys.exit(ret)

if __name__ == '__main__':
  sys.exit(main())
