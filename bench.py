import timeit

from dictproxy import DictProxyType

def test(test_str, setup_str):
    count = 10000000
    time = timeit.timeit(test_str, setup='from dictproxy import DictProxyType; ' + setup_str, number=count)
    print("Time for %40s: %s" % (test_str, format_time(time / count)))

def format_time(time):
    if time < 1e-9:
        time *= 1e9
        unit = 'p'
    elif time < 1e-6:
        time *= 1e9
        unit = 'n'
    elif time < 1e-3:
        time *= 1e9
        unit = 'u'
    elif time < 1:
        time *= 1e9
        unit = 'u'
    else:
        unit = 's'
    return '% 3.3f%ss' % (time, unit)

test("DictProxyType(a)", 'a={1: 2}')
test("a[1]", 'a={1: {2: 3}}; p=DictProxyType(a)')
test("p[1]", 'a={1: {2: 3}}; p=DictProxyType(a)')
test("a.get(1)", 'a={1: {2: 3}}; p=DictProxyType(a)')
test("p.get(1)", 'a={1: {2: 3}}; p=DictProxyType(a)')
test("rp[1]", 'a={1: {2: 3}}; rp=DictProxyType(a, 1)')
test("srp[1]", 'a={1: {2: 3}};\nclass sc(DictProxyType): pass\nsrp=sc(a, 1)')
test("srpo[1]", 'a={1: {2: 3}};\nclass sc(DictProxyType):\n\tdef __init__(self, args, args2): pass\nsrpo=sc(a, 1)')
