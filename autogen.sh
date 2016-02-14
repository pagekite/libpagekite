#!/bin/sh
mkdir -p m4
exec autoreconf --force --install -I m4
