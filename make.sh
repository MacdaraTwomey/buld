#!/bin/bash

make 2>&1 | sed 's/\/mnt\/c/c:/g'