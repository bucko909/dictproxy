from dictproxy import DictProxyType

def test_create():
    d = {}
    dp = DictProxyType(d)
    assert dp.keys() == []
    assert dp.values() == []
    assert dp.items() == []
    assert len(dp) == 0

def test_create_nonempty():
    o = object()
    d = {1: o}
    dp = DictProxyType(d)
    assert 1 in dp
    assert dp[1] is o
    assert dp.get(1) is o
    assert dp.keys() == [1]
    assert dp.values() == [o]
    assert len(dp) == 1

def test_insert():
    d = {}
    dp = DictProxyType(d)
    o = object()
    assert dp.get(1, o) is o
    assert 1 not in dp
    d[1] = o
    assert 1 in dp
    assert dp[1] is o
    assert dp.get(1) is o
    assert dp.keys() == [1]
    assert dp.values() == [o]
    assert len(dp) == 1

def test_del():
    o = object()
    o2 = object()
    d = {1: o}
    dp = DictProxyType(d)
    del d[1]
    assert dp.get(1, o2) is o2
    assert 1 not in dp
    assert dp.keys() == []
    assert dp.values() == []
    assert dp.items() == []
    assert len(dp) == 0

def test_readonly():
    dp = DictProxyType({})
    try:
        dp[1] = 1
        raise Exception('should not setting')
    except TypeError:
        pass
    try:
        del dp[1]
        raise Exception('should not allow deletion')
    except TypeError:
        pass

def test_raw():
    d = {}
    dp = DictProxyType(d)
    assert dp._raw_dict is d

def test_recursion():
    d = {1: {2: 3}, 4: 5}
    dp = DictProxyType(d, 1)
    assert dp._embed_depth == 1
    assert dp[1]._embed_depth == 0
    assert type(dp[1]) is DictProxyType
    assert dp[1][2] == 3
    try:
        dp[4]
        raise Exception('should not dereference non-dict')
    except TypeError:
        pass

def test_subclass_recursion():
    class SubClass(DictProxyType, object):
        def __new__(cls, _dict, _embed_depth):
            return super(cls, SubClass).__new__(cls, _dict, _embed_depth)
        def __init__(self, _dict, embed_depth):
            self.x = embed_depth + 1
    d = {1: {2: 3}}
    dp = SubClass(d, 1)
    assert type(dp[1]) is SubClass
    assert dp.x == 2
    assert dp[1].x == 1

def test_dict_subclass():
    class SubClass(dict, object):
        pass
    d = SubClass([(1, 2)])
    dp = DictProxyType(d)
    assert dp[1] == 2
