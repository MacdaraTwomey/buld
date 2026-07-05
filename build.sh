#!/bin/bash

# Usage: 
# build.sh win [linux-path] 
# build.sh linux [win-path | network-path]
# build.sh msvc

# ------------ Argument handling -------------------------------------------------
ArgCount=$#
Root=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

OS=$1
if [[ $ArgCount != 1 ]] && [[ $ArgCount != 2 ]]; then echo "Usage: build.sh <os>" && exit 1; fi
if [[ $ArgCount == 2 ]]; then OutputPathType=$2; fi

# ------------ Flags and sources -------------------------------------------------
Source="$Root/buld.cpp"
Include="-I $Root/src"
CommonFlags=""
Executable=`basename $Root`

ClangFlags="-g -std=c++20 -fno-strict-aliasing"
#ClangFlags+=" -fsanitize=address"
#ClangFlags+=" -Wall -pedantic-errors -Wno-unused-variable -Wno-gnu-anonymous-struct -Wno-writable-strings"
#ClangFlags+=" -Wno-nested-anon-types -Wno-gnu-zero-variadic-macro-arguments -Wno-missing-braces"
#ClangFlags+="  -Wno-unused-function -Wno-language-extension-token -Wno-deprecated-declarations"
#ClangFlags+=" -fdiagnostics-color"    # forces colour even when piping to sed

ClFlags="-std:c++latest /Zc:preprocessor -Zc:strictStrings- -D_CRT_SECURE_NO_WARNINGS -W3 -wd5105 -wd4201 -wd4505 -INCREMENTAL:NO -FC -EHs- -nologo -Zi"

# ------------ Boilerplate -------------------------------------------------------
if [[ $OS == "win" ]];   then Compiler="clang++.exe" Out="-o "; fi
if [[ $OS == "linux" ]]; then Compiler="clang++"     Out="-o "; fi
if [[ $OS == "msvc" ]];  then Compiler="cl.exe"      Out="-out:"; fi

function wsl_to_win() { echo "$1" | sed 's|/mnt/c/|c:/|g' ; }
function win_to_wsl() { echo "$1" | sed 's|c:/|/mnt/c/|g' ; }
function linux_to_net() { echo "$1" | sed 's|/home/mac/|\\\\wsl.localhost\\Ubuntu-22.04/home/mac/|g' ; }

if [[ $OS == "win" ]] || [[ $OS == "msvc" ]]; then Source=`wsl_to_win "$Source"`; fi
if [[ $OS == "win" ]] || [[ $OS == "msvc" ]]; then Include=`wsl_to_win "$Include"`; fi

if [[ $OS == "win" ]];  then Link="-fuse-ld=lld-link -l opengl32.lib -l kernel32.lib -l user32.lib -l gdi32.lib"; fi
if [[ $OS == "msvc" ]]; then Link="-link opengl32.lib kernel32.lib user32.lib gdi32.lib"; fi

Flags="$CommonFlags"
if [[ $OS == "linux" ]] || [[ $OS == "win" ]]; then Flags+=" $ClangFlags"; fi
if [[ $OS == "msvc" ]]; 					   then Flags+=" $ClFlags"; fi

# ------------ Compile -----------------------------------------------------------
mkdir -p "$Root/target"
pushd "$Root/target" > /dev/null

CMD="$Compiler $Source $Include $Flags $Link $Out$Executable"
echo "$CMD"

if [[ $OutputPathType == "win-path" ]]
then
	OUTPUT=`eval "$CMD" 2>&1`
	wsl_to_win "$OUTPUT"
elif [[ $OutputPathType == "linux-path" ]]
then
	OUTPUT=`eval "$CMD" 2>&1`
	win_to_wsl "$OUTPUT"
elif [[ $OutputPathType == "network-path" ]]
then
	OUTPUT=`eval "$CMD" 2>&1`
	linux_to_net "$OUTPUT"
else
	eval "$CMD"
fi

popd > /dev/null
