#!/bin/sh

PROJECT_HOME=`pwd`

if [ ! -d '.cscope' ]; then
    mkdir .cscope
fi

rm -rf ${PROJECT_HOME}/.cscope/cscope.files ${PROJECT_HOME}/.cscope/cscope.files

find ${PROJECT_HOME}/ -name '*.[ch]' > ${PROJECT_HOME}/.cscope/cscope.files

cscope -b -q -i ${PROJECT_HOME}/.cscope/cscope.files
mv cscope* .cscope/
