from setuptools import setup, find_packages, Extension
from setuptools.command.build_ext import build_ext
import sys
import setuptools
import glob
import os.path


## Please specify ITensor location.
__itensor_dir__ = 'external/itensor'


__version__ = '0.0.1'


class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __str__(self):
        import pybind11
        return pybind11.get_include()


ext_modules = [
    Extension(
        'qcircuit._core',
        # Sort input source files to ensure bit-for-bit reproducible builds
        # (https://github.com/pybind/python_example/pull/53)
        sorted(glob.glob('src/*.cpp')),
        include_dirs=[
            # Path to pybind11 headers
            get_pybind_include(),
            'include',
            __itensor_dir__,
        ],
        library_dirs=[os.path.join(__itensor_dir__, 'lib')],
        libraries=['itensor', 'lapack', 'blas', 'pthread'],
        language='c++'
    ),
]


# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile
    import os
    with tempfile.NamedTemporaryFile('w', suffix='.cpp', delete=False) as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        fname = f.name
    try:
        compiler.compile([fname], extra_postargs=[flagname])
    except setuptools.distutils.errors.CompileError:
        return False
    finally:
        try:
            os.remove(fname)
        except OSError:
            pass
    return True


def cpp_flag(compiler):
    """Return the -std=c++[17/20] compiler flag.
    The newer version is preferred over c++17 (when it is available).
    """
    flags = ['-std=c++17', 'std=c++20']

    for flag in flags:
        if has_flag(compiler, flag):
            return flag

    raise RuntimeError('Unsupported compiler -- at least C++17 support '
                       'is needed!')


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""

    c_opts = {
        'msvc': ['/EHsc'],
        'unix': [],
    }
    l_opts = {
        'msvc': [],
        'unix': [],
    }

    if sys.platform == 'darwin':
        darwin_opts = ['-stdlib=libc++', '-mmacosx-version-min=10.7']
        c_opts['unix'] += darwin_opts
        l_opts['unix'] += darwin_opts

    def build_extensions(self):
        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        link_opts = self.l_opts.get(ct, [])
        if ct == 'unix':
            opts.append(cpp_flag(self.compiler))
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')

        for ext in self.extensions:
            ext.define_macros = [('VERSION_INFO', '"{}"'.format(self.distribution.get_version()))]
            ext.extra_compile_args = opts
            ext.extra_link_args = link_opts
        build_ext.build_extensions(self)


setup(
    name='qcircuit',
    version=__version__,
    author='',
    author_email='',
    url='',
    description='',
    long_description='',
    ext_modules=ext_modules,
    setup_requires=['pybind11>=2.5.0'],
    cmdclass={'build_ext': BuildExt},
    packages=find_packages(),
    install_requires=[
          'qiskit',
    ],
    zip_safe=False,
)
