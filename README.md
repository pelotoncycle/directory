# directory
Python bindings to opendir/readdir plus parallel find

Python's `os.walk` is really awesome: it is simple to use, efficient
and of course pythonic.  It has one major flaw: its single threaded
and is only able to keep a single I/O operation in flight at a time.

At Peloton our leaderboard database is large, large enough that the
directory data for a randomly read ride is unlikely to be cached in
RAM when you request it.  With one file per second, per ride, we have
enough directory data that `os.walk` and `find` both take nearly an
hour to generate a mere file listing, in spite of having an SSD backed
EBS volume provisioned for the Amazon's maximum IO rate of 20kops/sec. 

But what if we could maintain more than one operation in flight at a
time?  Amazon's TOS states that you cannot expect to achieve your
maximum I/O rate unless you maintain 5 in-flight requests at a time.

To test this we created this python module `directory` to expose the
low level API calls `opendir` and `readdir`; by releasing the Global
Interpreter Lock around `opendir` and `readdir` calls and building a
parallel tree walker `pfind` we're able to keep more than one I/O
operation relating to the directory scan in flight at a time.

## Usage

To start a directory scan, import `Directory` from the `directory`
module, instantiate it and iterable over it like this:

```
>>> from directory import Directory

>>> for entry in Directory('.'):
...    print entry
```

Assuming run from this module's github repo, you'll see output that
looks something like this:

```
<Entry(path='.', d_ino=40241259, d_type=DT_DIR, d_name='dist')>
<Entry(path='.', d_ino=39848050, d_type=DT_REG, d_name='setup.py')>
<Entry(path='.', d_ino=40241246, d_type=DT_DIR, d_name='scripts')>
<Entry(path='.', d_ino=39848051, d_type=DT_REG, d_name='dirmodule.c~')>
<Entry(path='.', d_ino=39979075, d_type=DT_DIR, d_name='build')>
<Entry(path='.', d_ino=39848044, d_type=DT_DIR, d_name='.')>
<Entry(path='.', d_ino=39848053, d_type=DT_REG, d_name='directorymodule.c~')>
<Entry(path='.', d_ino=39848060, d_type=DT_REG, d_name='1')>
<Entry(path='.', d_ino=39979082, d_type=DT_DIR, d_name='directory.egg-info')>
<Entry(path='.', d_ino=39848047, d_type=DT_REG, d_name='setup.py~')>
<Entry(path='.', d_ino=39848052, d_type=DT_REG, d_name='directorymodule.c')>
<Entry(path='.', d_ino=40110319, d_type=DT_DIR, d_name='tests')>
<Entry(path='.', d_ino=35127306, d_type=DT_DIR, d_name='..')>
```

Each of the returned `Entry` objects exposes the underlying `dirent`
fields through the getter methods `d_ino`, `d_type` and `d_name`.  It
also exposes the additional methods `path` to return the full path of
the node and `directory()` which returns a new `Directory` object for
the entry if its a directory, or `None` otherwise. 

```
>>> for entry in Directory('.'):
...     if entry.d_name() == 'test': d = entry.directory()
>>> for entry in d:
...     print entry
<Entry(path='./tests', d_ino=40110322, d_type=DT_REG, d_name='test_directorymodule.pyc')>
<Entry(path='./tests', d_ino=40110319, d_type=DT_DIR, d_name='.')>
<Entry(path='./tests', d_ino=40110331, d_type=DT_REG, d_name='test_directorymodule.py')>
<Entry(path='./tests', d_ino=40110320, d_type=DT_DIR, d_name='data')>
<Entry(path='./tests', d_ino=39848044, d_type=DT_DIR, d_name='..')>
```

## pfind

The `pfind` utility is a very simple parallel file finder; while our
eventual goal is to have feature parity with the unix `find` utility,
this initial version serves only as a demonstration of how to scan a
directory with multiple concurrent inflight I/O operations.

`pfind` accepts a single parameter, the path of the directory tree to
scan, and emits to `stdout` a tab delimited text file listing the
`inode` number and file path.  If run against this module's git
repository, it produces something like this:

`# pfind directory
39979103        ./setup.py
39979106        ./.gitignore
39979100        ./directorymodule.c
40634076        ./tests/test_directorymodule.pyc
40504803        ./scripts/pfind
39979098        ./README.md
39979090        ./.git/packed-refs
40634111        ./tests/test_directorymodule.py
39979088        ./.git/description
40372661        ./.git/logs/HEAD
40634128        ./tests/data/README
40634114        ./tests/data/d2/3.txt
40634117        ./tests/data/d2/2.txt
40634130        ./tests/data/d1/1.txt
40372638        ./.git/info/exclude
39979096        ./.git/config
39979097        ./.git/HEAD
39979089        ./.git/index
40504789        ./.git/objects/8b/36a7b29514f01add9c709554aeb7e844daf57a
...
```
