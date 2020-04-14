import os
from setuptools import setup, find_packages


if __name__=='__main__':
    setup(
        name='tomato',
        version='0.0.1',
        author='Sviatoslav Abakumov, Racle',
        packages=find_packages('.'),
        include_package_data=True,
        entry_points={
            'console_scripts': [
                'tomato = clock.main:main',
            ]},
        install_requires=[
        'attrs',
            ],
        classifiers=[
                'Development Status :: 4 - Beta',
                'Environment :: Console',
                'Operating System :: Linux',
                "Programming Language :: Python :: 3.6",
                'Intended Audience :: Developers',
                'Topic :: Utilities',
            ]
    )