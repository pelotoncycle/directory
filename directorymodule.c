#include <Python.h>
#include <dirent.h>
#include <fcntl.h>
#include <fileobject.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

PyObject *dt_blk = NULL;
PyObject *dt_chr = NULL;
PyObject *dt_dir = NULL;
PyObject *dt_fifo = NULL;
PyObject *dt_lnk = NULL;
PyObject *dt_reg = NULL;
PyObject *dt_sock = NULL;
PyObject *dt_unknown = NULL;


typedef struct _directory_object DirectoryObject;
struct _directory_object {
  PyObject HEAD;
  int dirp;
  PyObject *path;
};

typedef struct _directory_iter_object DirectoryIterObject;
struct _directory_iter_object {
  PyObject HEAD;
  DIR *dirp;
  PyObject *path;
};

typedef struct _entryobject EntryObject;
struct _entryobject {
  PyObject HEAD;
  struct dirent dirent;
  PyObject *directory_iter;
  PyObject *d_ino;
  PyObject *d_type;
  PyObject *d_name;
};


PyObject *
make_new_directory(PyTypeObject *type, char *path);

PyObject *
make_new_directory_iter(PyTypeObject *type, PyObject *path);

PyObject *
make_new_entry(PyTypeObject *type, struct dirent *entry, PyObject *directory_iter);

static void directory_type_dealloc(DirectoryObject *dir) {
  Py_XDECREF(dir->path);
  if (dir->dirp != -1) {
    Py_BEGIN_ALLOW_THREADS
    close(dir->dirp);
    Py_END_ALLOW_THREADS
  }
  PyObject_Del(dir);
}

static void directory_iter_type_dealloc(DirectoryIterObject *di) { 
  if (di->dirp) {
    Py_BEGIN_ALLOW_THREADS
    closedir(di->dirp);
    Py_END_ALLOW_THREADS
  }
  Py_XDECREF(di->path);
  PyObject_Del(di);
}

static void entrytype_dealloc(EntryObject *entry) {
   Py_XDECREF(entry->directory_iter);
   Py_XDECREF(entry->d_ino);
   Py_XDECREF(entry->d_type);
   Py_XDECREF(entry->d_name);
   PyObject_Del(entry);
}


static int
directory_init(DirectoryObject *self, PyObject *args, PyObject **kwargs) 
{
  return 0;
}

static int
directory_iter_init(DirectoryIterObject *self, PyObject *args, PyObject **kwargs) 
{
  return 0;
}

static int 
entry_init(EntryObject *self, PyObject *args, PyObject **kwargs)
{
  return 0;
}

static PyObject *
directory_new(PyTypeObject *type, PyObject *args, PyObject **kwargs) 
{
  static char *kwlist[] = {"path",  NULL};
  char *path;
  if (!PyArg_ParseTupleAndKeywords(args, (PyObject *)kwargs, "|z", kwlist, &path))
    goto error;
  return make_new_directory(type, path);
 error:
  return NULL;
}



static PyObject *
directory_iter(DirectoryObject *dir) 
{
  return make_new_directory_iter(NULL, dir->path);
}

static PyObject *
directory_iter_next(DirectoryIterObject *di)
{
  struct dirent entry;
  struct dirent *result;
  int error_code;
  Py_BEGIN_ALLOW_THREADS
  error_code = readdir_r(di->dirp, &entry, &result);
  Py_END_ALLOW_THREADS
  if (error_code)
    return PyErr_SetFromErrno(PyExc_IOError);
  
  if (result) {
    PyObject *bye = make_new_entry(NULL, &entry, (PyObject *)di);
    return bye;
  }
  else {
    
    return NULL;
  }
}

PyObject *
directory_repr(DirectoryObject *self) {
  char *buffer = NULL;
  static const char *template = "<Directory(path=%s)>";
  PyObject *repr = PyObject_Repr(self->path);
  if (!repr)
    goto error;
  buffer = alloca(PyString_Size(self->path) + strlen(template) + 1);
  sprintf(buffer, template, PyString_AsString(repr));
  Py_DECREF(repr);
  return PyString_FromString(buffer);
 error:
  return NULL;
}


PyObject *
directory_iter_repr(DirectoryObject *self) {
  char *buffer = NULL;
  static const char *template = "<DirectoryIter(path=%s)>";
  PyObject *repr = PyObject_Repr(self->path);
  if (!repr)
    goto error;
  buffer = alloca(PyString_Size(repr) + strlen(template) + 1);
  sprintf(buffer, template, PyString_AsString(repr));
  Py_DECREF(repr);
  return PyString_FromString(buffer);
 error:
  return NULL;
}


PyObject *
entry_d_name(EntryObject *self, PyObject *_) {
  if (!self->d_name) 
    self->d_name = PyString_FromString((const char *)&self->dirent.d_name);
  Py_XINCREF(self->d_name);
  return self->d_name;
}

PyObject *
entry_path(EntryObject *self, PyObject *_){
  DirectoryIterObject *di = (DirectoryIterObject *)self->directory_iter;
  char *buffer = alloca(PyString_Size(di->path) + strlen((const char *)&self->dirent.d_name) + 2);
  strcpy(buffer, PyString_AsString(di->path));
  buffer[PyString_Size(di->path)] = '/';
  strcpy(buffer + PyString_Size(di->path) + 1, (const char *)&self->dirent.d_name);
  return PyString_FromString(buffer);
}

PyObject *
entry_directory(EntryObject *self, PyObject *_) {
  if (self->dirent.d_type != DT_DIR)
    Py_RETURN_NONE;

  DirectoryIterObject *di = (DirectoryIterObject *)self->directory_iter;
  char *buffer = alloca(PyString_Size(di->path) + strlen((const char *)&self->dirent.d_name) + 2);
  strcpy(buffer, PyString_AsString(di->path));
  buffer[PyString_Size(di->path)] = '/';
  strcpy(buffer + PyString_Size(di->path) + 1, (const char *)&self->dirent.d_name);
  return make_new_directory(NULL, buffer);
}

PyObject *
directory_open(DirectoryObject *self, PyObject *py_name) {
  FILE *fp = NULL;
  int fd = -1;
  char *name = NULL;
  char *path = NULL;
  if (!PyString_Check(py_name)) {
    PyErr_SetString(PyExc_TypeError,
		    "Directory.open(...) must be called with a str");
    return NULL;
  }
  path = PyString_AsString(self->path);
  name = PyString_AsString(py_name);
  Py_BEGIN_ALLOW_THREADS
  
  if (self->dirp == -1)
    self->dirp = open(path, O_DIRECTORY | O_RDONLY);
  if (self->dirp != -1)
    fd = openat(self->dirp, name, O_CREAT | O_RDWR, 0777);
  if (fd != -1)
    fp = fdopen(fd, "r+");
  Py_END_ALLOW_THREADS

  if (self->dirp == -1 || fd == -1 || !fp)
    return PyErr_SetFromErrno(PyExc_IOError);
  char *buffer = alloca(PyString_Size(self->path) + strlen(name) + 2);
  strcpy(buffer, PyString_AsString(self->path));
  buffer[PyString_Size(self->path)] = '/';
  strcpy(buffer + PyString_Size(self->path) + 1, name);

  return PyFile_FromFile(fp, buffer, "w+", fclose);
}

    
PyObject *
entry_repr(EntryObject *self) {
  PyObject *path_repr = NULL;
  PyObject *name = NULL;
  PyObject *name_repr = NULL;
  char *buffer = NULL;
  
  static const char *template = "<Entry(path=%s, d_ino=%lld, d_type=%s, d_name=%s)>";
  
  DirectoryIterObject *di = (DirectoryIterObject *)self->directory_iter;
  path_repr = PyObject_Repr(di->path);
  if (!path_repr)
    goto error;
  name = entry_d_name(self, NULL);
  if (!name)
    goto error;
  name_repr = PyObject_Repr(name);
    
  char *d_type = NULL;
  switch (self->dirent.d_type) {
    case DT_BLK:
      d_type = "DT_BLK";
      break;
    case DT_CHR:
      d_type = "DT_CHR";
      break;
    case DT_DIR:
      d_type = "DT_DIR";
      break;
    case DT_FIFO:
      d_type = "DT_FIFO";
      break;
    case DT_LNK:
      d_type = "DT_LNK";
      break;
    case DT_REG:
      d_type = "DT_REG";
      break;
    case DT_SOCK:
      d_type = "DT_SOCK";
      break;
    default:
      d_type = "DT_UNKNOWN";
      break;
    }
  buffer = alloca(strlen(template) +
		  strlen(self->dirent.d_name) + 
		  PyString_Size(path_repr) + 
		  PyString_Size(name_repr) + 
		  strlen(d_type) + 
		  30);
  sprintf(buffer, template, PyString_AsString(path_repr), self->dirent.d_ino, d_type, PyString_AsString(name_repr));
  
  Py_XDECREF(path_repr);
  Py_XDECREF(name);
  Py_XDECREF(name_repr);
  return PyString_FromString(buffer);
  
 error:
  Py_XDECREF(path_repr);
  Py_XDECREF(name);
  Py_XDECREF(name_repr);
  return NULL;
};

static PyMethodDef directory_methods[] = {
  {"open", (PyCFunction)directory_open, METH_O, NULL},
  {NULL, NULL}};

PyTypeObject DirectoryType = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  "Directory", /* tp_name */
  sizeof(DirectoryObject), /* tp_basicsize */
  0, /* tp_itemsize */
  (destructor)directory_type_dealloc, /* tp_dealloc */
  0, /* tp_print */
  0, /* tp_getattr */
  0, /* tp_setattr */
  0, /* tp_cmp */
  (PyObject *(*)(PyObject *))directory_repr, /* tp_repr */
  0, /* tp_as_number */
  0, /* tp_as_seqeunce */
  0, /* tp_mapping */
  (hashfunc)PyObject_HashNotImplemented, /*tp_hash */
  0, /* tp_call */
  0, /* tp_str */
  PyObject_GenericGetAttr, /* tp_getattro */
  0, /* tp_setattro */
  0, /* tp_as_buffer */
  Py_TPFLAGS_HAVE_ITER,	/* tp_flags */
  0, /* tp_doc */
  0, /* tp_traverse */
  0, /* tp_clear */
  0, /* tp_richcompare */
  0, /* tp_weaklistoffset */
  (PyObject *(*)(PyObject *))directory_iter, /* tp_iter */
  0, /* tp_iternext */
  directory_methods, /* tp_methods */
  0, /* tp_members */
  0, /* tp_genset */
  0, /* tp_base */
  0, /* tp_dict */
  0, /* tp_descr_get */
  0, /* tp_descr_set */
  0, /* tp_dictoffset */
  (initproc)directory_init, /* tp_init */
  PyType_GenericAlloc, /* tp_alloc */
  (PyObject *(*)(struct _typeobject *, PyObject *, PyObject *))directory_new, /* tp_new */
  0,
};


PyTypeObject DirectoryIterType = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  "DirectoryIter", /* tp_name */
  sizeof(DirectoryIterObject), /* tp_basicsize */
  0, /* tp_itemsize */
  (destructor)directory_iter_type_dealloc, /* tp_dealloc */
  0, /* tp_print */
  0, /* tp_getattr */
  0, /* tp_setattr */
  0, /* tp_cmp */
  (PyObject *(*)(PyObject *))directory_iter_repr, /* tp_repr */
  0, /* tp_as_number */
  0, /* tp_as_seqeunce */
  0, /* tp_mapping */
  (hashfunc)PyObject_HashNotImplemented, /*tp_hash */
  0, /* tp_call */
  0, /* tp_str */
  PyObject_GenericGetAttr, /* tp_getattro */
  0, /* tp_setattro */
  0, /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER, /* tp_flags */
  0, /* tp_doc */
  0, /* tp_traverse */
  0, /* tp_clear */
  0, /* tp_richcompare */
  0, /* tp_weaklistoffset */
  PyObject_SelfIter, /* tp_iter */
  (PyObject *(*)(PyObject *))directory_iter_next, /* tp_iternext */
  0, /* tp_methods */
  0, /* tp_members */
  0, /* tp_genset */
  0, /* tp_base */
  0, /* tp_dict */
  0, /* tp_descr_get */
  0, /* tp_descr_set */
  0, /* tp_dictoffset */
  (initproc)directory_iter_init, /* tp_init */
  PyType_GenericAlloc, /* tp_alloc */
  0, /* tp_new */
  0,
};


 PyObject *
   entry_d_ino(EntryObject *self, PyObject *_) {
  if (!self->d_ino)
    self->d_ino = PyInt_FromLong(self->dirent.d_ino);
  Py_XINCREF(self->d_ino);
  return self->d_ino;
}

PyObject *
entry_pfind_str(EntryObject *self, PyObject *_) {
  DirectoryIterObject *di = (DirectoryIterObject *)self->directory_iter;
  char *buffer = alloca(PyString_Size(di->path) + 300 ); // 256 for name, extra for number

  sprintf(buffer, "%lld\t%s/%s\n", self->dirent.d_ino, PyString_AsString(di->path), (const char *)&self->dirent.d_name);
  return PyString_FromString(buffer);
}

PyObject *
entry_open(EntryObject *self, PyObject *_) {
  DirectoryIterObject *di = (DirectoryIterObject *)self->directory_iter;
  int dfd = dirfd(di->dirp);
  int fd = -1;
  FILE *fp = NULL;

  if (dfd == -1)
    return PyErr_SetFromErrnoWithFilename(PyExc_IOError, PyString_AsString(di->path));

  PyObject *path = entry_path(self, NULL);
  if (!path)
    goto error;

  Py_BEGIN_ALLOW_THREADS
    fd = openat(dirfd(di->dirp), self->dirent.d_name, O_CREAT | O_RDWR, 0777);
  Py_END_ALLOW_THREADS

  if (fd == -1) {
    PyErr_SetFromErrnoWithFilename(PyExc_IOError, PyString_AsString(path));
    goto error;
  }
  fp = fdopen(fd, "r+");
  if (!fp) {
    PyErr_SetFromErrnoWithFilename(PyExc_IOError, PyString_AsString(path));
    goto error;
  }

  PyObject *file = PyFile_FromFile(fp, PyString_AsString(path), "r+", fclose);
  Py_DECREF(path);
  return file;

 error:
  Py_XDECREF(path);

  if (fp) {
    Py_BEGIN_ALLOW_THREADS
    fclose(fp);
    Py_END_ALLOW_THREADS
  }
  return NULL;
}

PyObject *
entry_d_type(EntryObject *self, PyObject *_) {
  if (!self->d_type) {
    switch (self->dirent.d_type) { 
    case DT_BLK:
      self->d_type = dt_blk;
      break;
    case DT_CHR:
      self->d_type = dt_chr;
      break;
    case DT_DIR:
      self->d_type = dt_dir;
      break;
    case DT_FIFO:
      self->d_type = dt_fifo;
      break;
    case DT_LNK:
      self->d_type = dt_lnk;
      break;
    case DT_REG:
      self->d_type = dt_reg;
      break;
    case DT_SOCK:
      self->d_type = dt_sock;
      break;
    default:
      self->d_type = dt_unknown;
      break;
    }
    Py_INCREF(self->d_type);
  }
  Py_INCREF(self->d_type);
  return self->d_type;
}



static PyMethodDef entry_methods[] = {
  {"d_ino", (PyCFunction)entry_d_ino, METH_NOARGS, NULL},
  {"d_name", (PyCFunction)entry_d_name, METH_NOARGS, NULL},
  {"d_type", (PyCFunction)entry_d_type, METH_NOARGS, NULL},
  {"directory", (PyCFunction)entry_directory, METH_NOARGS, NULL},
  {"open", (PyCFunction)entry_open, METH_NOARGS, NULL},
  {"path", (PyCFunction)entry_path, METH_NOARGS, NULL},
  {"pfind_str", (PyCFunction)entry_pfind_str, METH_NOARGS, NULL},
  {NULL, NULL}};

PyTypeObject EntryType = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  "Entry", /* tp_name */
  sizeof(EntryObject), /* tp_basicsize */
  0, /* tp_itemsize */
  (destructor)entrytype_dealloc, /* tp_dealloc */
  0, /* tp_print */
  0, /* tp_getattr */
  0, /* tp_setattr */
  0, /* tp_cmp */
  (PyObject *(*)(PyObject *))entry_repr, /* tp_repr */
  0, /* tp_as_number */
  0, /* tp_as_seqeunce */
  0, /* tp_mapping */
  (hashfunc)PyObject_HashNotImplemented, /*tp_hash */
  0, /* tp_call */
  0, /* tp_str */
  PyObject_GenericGetAttr, /* tp_getattro */
  0, /* tp_setattro */
  0, /* tp_as_buffer */
  0, /* tp_flags */
  0, /* tp_doc */
  0, /* tp_traverse */
  0, /* tp_clear */
  0, /* tp_richcompare */
  0, /* tp_weaklistoffset */
  0, /* tp_iter */
  0, /* tp_iternext */
  entry_methods, /* tp_methods */
  0, /* tp_members */
  0, /* tp_genset */
  0, /* tp_base */
  0, /* tp_dict */
  0, /* tp_descr_get */
  0, /* tp_descr_set */
  0, /* tp_dictoffset */
  (initproc)entry_init, /* tp_init */
  PyType_GenericAlloc, /* tp_alloc */
  0, /* tp_new */
  0,
};

PyObject *
make_new_entry(PyTypeObject *type, struct dirent *entry, PyObject *directory_iter) {
  EntryObject *entry_object = PyObject_New(EntryObject, &EntryType);
  if (!entry_object) 
    goto error;

  memcpy(&entry_object->dirent, entry, sizeof(struct dirent));
  entry_object->directory_iter = directory_iter;
  Py_INCREF(directory_iter);
  entry_object->d_ino = NULL;
  entry_object->d_type = NULL;
  entry_object->d_type = NULL;
  entry_object->d_name = NULL;
  return (PyObject *)entry_object;
 error:
  return NULL;
}


PyObject *
make_new_directory(PyTypeObject *type, char *path) {
  DirectoryObject *directory = PyObject_New(DirectoryObject, &DirectoryType);
  PyObject *path_object = NULL;
  path_object = PyString_FromString(path);

  if (!directory) 
    goto error;

  if (!path_object)
    goto error;
  if (!path) {
    Py_INCREF(Py_None);
    path_object = Py_None;
  } else {
    path_object = PyString_FromString(path);
    if (!path_object)
      goto error;
  }
      
  directory->path = path_object;
  directory->dirp = -1;
  return (PyObject *)directory;
 error:
  Py_XDECREF(path_object);
  Py_XDECREF(directory);
  if (path)
    return PyErr_SetFromErrnoWithFilename(PyExc_IOError, path);
  return PyErr_SetFromErrno(PyExc_IOError);
}

PyObject *
make_new_directory_iter(PyTypeObject *type, PyObject *path_object) {
  int fd = -1;
  DIR *dir = NULL;
  if (!type)
    type = &DirectoryIterType;
  if (!path_object)
    path_object = Py_None;
  Py_INCREF(path_object);
  Py_BEGIN_ALLOW_THREADS
  fd = open(PyString_AsString(path_object), O_RDONLY | O_DIRECTORY);
  if (fd != -1)
    dir = fdopendir(fd);
  Py_END_ALLOW_THREADS
  if (fd == -1)
    goto unix_error;
  if (!dir) {
    Py_BEGIN_ALLOW_THREADS
    close(fd);
    Py_END_ALLOW_THREADS
    goto unix_error;
  }
  DirectoryIterObject *directory_iter = PyObject_New(DirectoryIterObject, &DirectoryIterType);
  if (!directory_iter)
    goto python_error; 
  directory_iter->dirp = dir;
  Py_INCREF(path_object);
  directory_iter->path = path_object;
  return (PyObject *)directory_iter;
 unix_error:
  if (path_object)
    PyErr_SetFromErrnoWithFilename(PyExc_IOError, PyString_AsString(path_object));
  else
    PyErr_SetFromErrno(PyExc_IOError);
 python_error:
  Py_XDECREF(path_object);
  return NULL;
}
  

static PyMethodDef directorymodule_methods[] = {
  {NULL, NULL, 0, NULL }};

PyMODINIT_FUNC
initdirectory(void) {
  PyObject *m = Py_InitModule("directory", directorymodule_methods);
  Py_INCREF(&DirectoryType);
  Py_INCREF(&DirectoryIterType);
  Py_INCREF(&EntryType);
  PyModule_AddObject(m, "Directory", (PyObject *)&DirectoryType);
  PyModule_AddObject(m, "DirectoryIter", (PyObject *)&DirectoryIterType);
  PyModule_AddObject(m, "Entry", (PyObject *)&EntryType);
  PyModule_AddObject(m, "DT_BLK", dt_blk = PyInt_FromLong(DT_BLK));
  PyModule_AddObject(m, "DT_CHR", dt_chr = PyInt_FromLong(DT_CHR));
  PyModule_AddObject(m, "DT_DIR", dt_dir = PyInt_FromLong(DT_DIR));
  PyModule_AddObject(m, "DT_FIFO", dt_fifo = PyInt_FromLong(DT_FIFO));
  PyModule_AddObject(m, "DT_LNK", dt_lnk = PyInt_FromLong(DT_LNK));
  PyModule_AddObject(m, "DT_REG", dt_reg = PyInt_FromLong(DT_REG));
  PyModule_AddObject(m, "DT_SOCK", dt_sock = PyInt_FromLong(DT_SOCK));
  PyModule_AddObject(m, "DT_UNKNOWN", dt_unknown = PyInt_FromLong(DT_UNKNOWN));
  Py_INCREF(dt_blk);
  Py_INCREF(dt_chr);
  Py_INCREF(dt_dir);
  Py_INCREF(dt_fifo);
  Py_INCREF(dt_lnk);
  Py_INCREF(dt_reg);
  Py_INCREF(dt_sock);
  Py_INCREF(dt_unknown);

}
    
