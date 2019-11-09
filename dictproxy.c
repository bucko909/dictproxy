#define PY_SSIZE_T_CLEAN
#include <Python.h>

typedef struct {
    PyObject_HEAD
    PyObject *dict;
    long embed_depth; // ignored on plain proxies
} dictproxyobject;

static PyObject *
_dictproxy_new(PyTypeObject *type, PyObject *dict, int embed_depth);

static Py_ssize_t
dictproxy_len(dictproxyobject *proxy)
{
    return PyObject_Size(proxy->dict);
}

// At the saving of ~60ms per top-level lookup, this means we don't allow
// overriding of either __init__ or __new__.
//
// Most of the saving is avoiding creation of the kwargs dictionary, needed for
// both.
#define QUICKINIT

#include <stdio.h>

PyTypeObject DictProxyType;

static PyObject *
_dictproxy_get_fixreturn(dictproxyobject *proxy, PyObject *obj)
{
    if (proxy->embed_depth == 0) {
        Py_INCREF(obj);
        return obj;
    } else {
        PyTypeObject *type = Py_TYPE(proxy);
        PyObject *wrapper;
        if ((void *)type->tp_new == (void *)DictProxyType.tp_new
                && (void *)type->tp_init == (void *)DictProxyType.tp_init) {
            // Fast path init when we understand what's going on.
            wrapper = _dictproxy_new(Py_TYPE(proxy), obj, proxy->embed_depth - 1);
            if (wrapper == NULL) {
                return NULL;
            }
        } else {
            // This path is about 50ns slower than the above one on my test
            // setup; that's 100ns per call rather than 50ns.
            // If you've overridden __new__ and/or __init__ in Python, add
            // another 100ms per override per call.
#if PY_MAJOR_VERSION >= 3
            PyObject *embed_depth = PyLong_FromLong(proxy->embed_depth - 1);
#else
            PyObject *embed_depth = PyInt_FromLong(proxy->embed_depth - 1);
#endif
            if (embed_depth == NULL) {
                return NULL;
            }
            PyObject *args = PyTuple_Pack(2, obj, embed_depth);
            if (args == NULL) {
                Py_DECREF(embed_depth);
                return NULL;
            }
            PyObject *kwargs = PyDict_New();
            if (kwargs == NULL) {
                Py_DECREF(args);
                Py_DECREF(embed_depth);
                return NULL;
            }

            PyTypeObject *type = Py_TYPE(proxy);
            wrapper = type->tp_new(type, args, kwargs);
            if (wrapper == NULL) {
                Py_DECREF(kwargs);
                Py_DECREF(args);
                Py_DECREF(embed_depth);
                return NULL;
            }
            type->tp_init(wrapper, args, kwargs);
            Py_DECREF(kwargs);
            Py_DECREF(args);
            Py_DECREF(embed_depth);
        }
        return wrapper;
    }
}

static PyObject *
dictproxy_getitem(dictproxyobject *proxy, PyObject *key)
{
    // In Py3 should be GetItemWithError. Check PyErr_Occurred.
    PyObject *obj = PyDict_GetItem(proxy->dict, key);
    if (obj == NULL) {
        PyErr_SetObject(PyExc_KeyError, key);
        return NULL;
    }

    return _dictproxy_get_fixreturn(proxy, obj);
}

static PyMappingMethods dictproxy_as_mapping = {
    .mp_length = (lenfunc)dictproxy_len,
    .mp_subscript = (binaryfunc)dictproxy_getitem,
};

static int
dictproxy_contains(dictproxyobject *proxy, PyObject *key)
{
    if (PyDict_CheckExact(proxy->dict))
        return PyDict_Contains(proxy->dict, key);
    else
        return PySequence_Contains(proxy->dict, key);
}

static PySequenceMethods dictproxy_as_sequence = {
    .sq_contains = (objobjproc)dictproxy_contains,
};

static PyObject *
dictproxy_get(dictproxyobject *proxy, PyObject *args)
{
    // Uses PyDict functions for speed!
    PyObject *key, *deflt;
    if (!PyArg_UnpackTuple(args, "get", 1, 2, &key, &deflt))
    {
        return NULL;
    }

    // In Py3 should be GetItemWithError. Check PyErr_Occurred.
    PyObject *obj = PyDict_GetItem(proxy->dict, key);
    if (obj == NULL) {
        Py_INCREF(deflt);
        return deflt;
    }

    return _dictproxy_get_fixreturn(proxy, obj);
}

static PyObject *
dictproxy_keys(dictproxyobject *proxy, PyObject *const *args, Py_ssize_t nargs)
{
    return PyDict_Keys(proxy->dict);
}

static PyObject *
dictproxy_values(dictproxyobject *proxy, PyObject *const *args, Py_ssize_t nargs)
{
    return PyDict_Values(proxy->dict);
}

static PyObject *
dictproxy_items(dictproxyobject *proxy, PyObject *const *args, Py_ssize_t nargs)
{
    return PyDict_Items(proxy->dict);
}

static PyMethodDef dictproxy_methods[] = {
    {"get",       (PyCFunction)(void(*)(void))dictproxy_get, METH_VARARGS,
     PyDoc_STR("D.get(k[,d]) -> D[k] if k in D, else d."
               "  d defaults to None.")},
    {"keys",      (PyCFunction)dictproxy_keys,               METH_NOARGS,
     PyDoc_STR("D.keys() -> list of D's keys")},
    {"values",    (PyCFunction)dictproxy_values,             METH_NOARGS,
     PyDoc_STR("D.values() -> list of D's values")},
    {"items",     (PyCFunction)dictproxy_items,              METH_NOARGS,
     PyDoc_STR("D.items() -> list of D's (key, value) pairs, as 2-tuples")},
    {0}
};

static PyObject *
dictproxy_get_raw_dict(dictproxyobject *proxy, void *closure)
{
    return proxy->dict;
}

static PyObject *
dictproxy_get_embed_depth(dictproxyobject *proxy, void *closure)
{
#if PY_MAJOR_VERSION >= 3
    return PyLong_FromLong(proxy->embed_depth);
#else
    return PyInt_FromLong(proxy->embed_depth);
#endif
}

static PyGetSetDef dictproxy_getsetters[] = {
    {"_raw_dict", (getter)dictproxy_get_raw_dict, NULL, NULL},
    {"_embed_depth", (getter)dictproxy_get_embed_depth, NULL, NULL},
    {NULL}
};

static void
dictproxy_dealloc(dictproxyobject *proxy)
{
    _PyObject_GC_UNTRACK(proxy);
    Py_DECREF(proxy->dict);
    PyObject_GC_Del(proxy);
}

static PyObject *
dictproxy_getiter(dictproxyobject *proxy)
{
    return PyObject_GetIter(proxy->dict);
}

static PyObject *
dictproxy_repr(dictproxyobject *proxy)
{
    return PyUnicode_FromFormat("<dictproxy object of %p>", (long long)proxy->dict);
}

static int
dictproxy_traverse(PyObject *self, visitproc visit, void *arg)
{
    dictproxyobject *proxy = (dictproxyobject *)self;
    Py_VISIT(proxy->dict);
    return 0;
}

static PyObject *
dictproxy_richcompare(dictproxyobject *v, PyObject *w, int op)
{
    return PyObject_RichCompare(v->dict, w, op);
}

static int
dictproxy_check_dict(PyObject *dict)
{
    if (!PyDict_Check(dict)) {
        PyErr_Format(PyExc_TypeError,
                    "DictProxyType() argument must be a dict, not %s",
                    Py_TYPE(dict)->tp_name);
        return -1;
    }
    return 0;
}

static PyObject *
dictproxy_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *dict, *o_depth;
    o_depth = NULL;
    if (!PyArg_UnpackTuple(args, "new", 1, 2,
                            &dict, &o_depth))
    {
        return NULL;
    }

    long embed_depth;
    if (o_depth == NULL) {
        embed_depth = 0;
#if PY_MAJOR_VERSION < 3
    } else if (PyInt_Check(o_depth)) {
        embed_depth = PyInt_AsLong(o_depth);
        if (embed_depth == -1 && PyErr_Occurred()) {
            return NULL;
        }
#endif
    } else if (PyLong_Check(o_depth)) {
        embed_depth = PyLong_AsLong(o_depth);
        if (embed_depth == -1 && PyErr_Occurred()) {
            return NULL;
        }
    } else {
        PyErr_Format(PyExc_TypeError,
                    "DictProxyType() embed_depth must be an int, not %s",
                    Py_TYPE(o_depth)->tp_name);
        return NULL;
    }

    return _dictproxy_new(type, dict, embed_depth);
}

static PyObject *
_dictproxy_new(PyTypeObject *type, PyObject *dict, int embed_depth)
{
    if (embed_depth < 0) {
        PyErr_Format(PyExc_ValueError,
                    "DictProxyType() embed_depth must be at least 0, not %i",
                    embed_depth);
        return NULL;
    }

    dictproxyobject *dictproxy;

    if (dictproxy_check_dict(dict) == -1) {
        return NULL;
    }

    dictproxy = (dictproxyobject *)type->tp_alloc(type, 0);
    if (dictproxy == NULL) {
        PyErr_Format(PyExc_MemoryError,
                    "Could not allocate DictProxyType");
        return NULL;
    }
    Py_INCREF(dict);
    dictproxy->dict = dict;
    dictproxy->embed_depth = embed_depth;
    return (PyObject *)dictproxy;
}

PyTypeObject DictProxyType = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "DictProxyType",
    .tp_basicsize = sizeof(dictproxyobject),
    .tp_dealloc = (destructor)dictproxy_dealloc,
    .tp_repr = (reprfunc)dictproxy_repr,
    .tp_as_sequence = &dictproxy_as_sequence,
    .tp_as_mapping = &dictproxy_as_mapping,
    .tp_str = (reprfunc)dictproxy_repr,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .tp_traverse = dictproxy_traverse,
    .tp_richcompare = (richcmpfunc)dictproxy_richcompare,
    .tp_iter = (getiterfunc)dictproxy_getiter,
    .tp_methods = dictproxy_methods,
    .tp_getset = dictproxy_getsetters,
    .tp_new = dictproxy_new,
};

#define NAME "dictproxy"

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef dictproxy_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = NAME,
    .m_size = -1,
};

#define INITERROR return NULL

PyMODINIT_FUNC
PyInit_dictproxy(void) {
#else

#define INITERROR return

void
initdictproxy(void) {
#endif

    PyObject *module;

    if (PyType_Ready(&DictProxyType) < 0) {
        INITERROR;
    }

#if PY_MAJOR_VERSION >= 3
    module = PyModule_Create(&dictproxy_module);
#else
    module = Py_InitModule(NAME, NULL);
#endif
    if (PyModule_AddObject(module, "DictProxyType", (PyObject *) &DictProxyType) < 0) {
        Py_DECREF(&DictProxyType);
        Py_DECREF(module);
        INITERROR;
    }

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}
