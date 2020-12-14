import setuptools
import os
import glob


setuptools.setup(
    name='PySubstringSearch',
    version='0.3.1',
    author='Gal Ben David',
    author_email='gal@intsights.com',
    url='https://github.com/Intsights/PySubstringSearch',
    project_urls={
        'Source': 'https://github.com/Intsights/PySubstringSearch',
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
        'Programming Language :: Python :: 3.9',
    ],
    keywords='substring pattern search suffix array c++',
    python_requires='>=3.6',
    zip_safe=False,
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
                '-std=c++17',
            ],
            include_dirs=[
                'src',
            ],
        ),
    ],
)
