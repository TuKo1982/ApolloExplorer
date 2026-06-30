#!/bin/bash

brew install qt

rm -r -f .qmake.stash
clear

qmake -recursive
make clean
make -j16
make clean

cp ApolloExplorerPC/icons/ApolloExplorer.icns ApolloExplorerPC/ApolloExplorer.app/Contents/Resources/ 

rm -r -f .qmake.stash Makefile ApolloExplorerPC/Makefile ApolloExplorerPC/qmake_qmake_qm_files.qrc
rm -r -f acp/Makefile AmigaIconReader/Makefile 
