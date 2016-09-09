try:
    from setuptools import setup, Extension
except ImportError:
    from distutils.core import setup, Extension

setup(name='directory',
      author = 'Adam DePrince',
      author_email = 'adam@pelotoncycle.com',
      url = 'https://github.com/pelotoncycle/dir',
      version='0.0.6',

      description="Expose opendir/readdir/closedir",
      scripts=[
          'scripts/pfind',
          ],
      ext_modules=(
          [
              Extension(
                  name='directory',
                  sources=['directorymodule.c']),
          ]))
