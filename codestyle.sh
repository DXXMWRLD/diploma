#!/bin/bash

find $1/ -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' | xargs clang-format -i -style=file
