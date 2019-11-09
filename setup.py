from distutils.core import setup, Extension

module1 = Extension('dictproxy',
                    sources = ['dictproxy.c'])

setup (name = 'dictproxy',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [module1])
