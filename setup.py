from distutils.core import setup, Extension

module1 = Extension('RaspiOled.oled',
                    sources = [ 'src/raspioled.c'])

setup(name = 'RaspiOled',
      version = '0.0',
      description = 'extention for i2c oled on raspberry pi',
      package_dir = { '': 'lib'},
      packages=['RaspiOled'],
      ext_modules = [module1])

