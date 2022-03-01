#!/bin/bash

cd ../pflayer
make clean
make

cd ../amlayer
make clean
make

cd ../dblayer
make clean
make