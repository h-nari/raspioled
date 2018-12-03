from distutils.core import setup, Extension

module1 = Extension('raspioled',
                    sources = [ 'raspioled.c'])

setup(name = 'PackageName',
      version = '0.0',
      description = 'extention for i2c oled on raspberry pi',
      ext_modules = [module1])

