import setuptools
import os
import glob


class GetPybind11Include:
    def __init__(
        self,
        user,
    ):
        self.user = user

    def __str__(
        self,
    ):
        import pybind11

        return pybind11.get_include(
            user=self.user,
        )


setuptools.setup(
    name='PySubstringSearch',
    version='0.2.4',
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
    ],
    keywords='substring pattern search suffix array c++',
    python_requires='>=3.6',
    zip_safe=False,
    install_requires=[
        'pybind11',
    ],
    setup_requires=[
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
                GetPybind11Include(False),
                GetPybind11Include(True),
            ]
        ),
    ],
)
