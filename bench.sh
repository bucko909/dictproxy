#!/bin/sh

set -o errexit

for VER in 2 3; do
	if ! [ -e env$VER ]; then
		virtualenv -p python$VER env$VER
		env$VER/bin/pip install pytest
	fi

	env$VER/bin/python setup.py install
	env$VER/bin/python bench.py
done
