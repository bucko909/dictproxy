Dict Proxy
==========

This is a simple C Python 2/3 module to provide fast readonly proxies to recursive dictionaries in C. It's based on `MappingProxyType` from Python3, but makes a strong assumption that it proxies a raw `dict` (or some subclass of it that doesn't override readonly methods). We can easily support this, but `.get()` has awful performance this way, and implementing it on everything but is inconcsistent.

Note that we *do* support subclassing `DictProxyType`, at some small performance penalty when doing lookups embedded dictionaries. If you also override either `__new__` or `__init__`, performance will tank -- do this only for debugging!
