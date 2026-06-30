#!/bin/bash

rm -r -f .qmake.stash
clear

qmake -recursive
make clean
make -j16
make clean

sudo cp ApolloExplorerPC/ApolloExplorer /opt/ApolloExplorer/bin/ApolloExplorer
sudo cp ApolloExplorerPC/ApolloExplorer.desktop /usr/share/applications/ApolloExplorer.desktop

rm -r -f .qmake.stash Makefile ApolloExplorerPC/Makefile ApolloExplorerPC/qmake_qmake_qm_files.qrc
rm -r -f acp/Makefile AmigaIconReader/Makefile 