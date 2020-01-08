import setuptools
import pybind11
import os
import glob


setuptools.setup(
    name='PySubstringSearch',
    version='0.2.2',
    author='Gal Ben David',
    author_email='wavenator@gmail.com',
    url='https://github.com/wavenator/PySubstringSearch',
    project_urls={
        'Source': 'https://github.com/wavenator/PySubstringSearch',
    },
    license='MIT',
    description='Python library for fast substring/pattern search written in C++ leveraging Suffix Array Algorithm',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    classifiers=[
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
    ],
    keywords='substring pattern search suffix array c++',
    python_requires='>=3.6',
    zip_safe=False,
    install_requires=[
        'pybind11',
    ],
    package_data={},
    include_package_data=True,
    ext_modules=[
        setuptools.Extension(
            name='pysubstringsearch',
            sources=glob.glob(
                pathname=os.path.join(
                    'src',
                    'pysubstringsearch.cpp',
                ),
            ),
            language='c++',
            extra_compile_args=[
                '-Ofast',
                '-std=c++17',
            ],
            extra_link_args=[
                '-lpthread',
            ],
            include_dirs=[
                'src',
                pybind11.get_include(False),
                pybind11.get_include(True),
            ]
        ),
    ],
)
