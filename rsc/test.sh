#!/bin/bash

EXE="$1 -gaZ"
ENAME=$(basename $1)
DIR="test"
OK=true

singleTest() {
	touch "$DIR/$2"
	$EXE $1 "$DIR/$2"
	if test -f "$DIR/$3"; then
		echo "'$ENAME $1' passed"
		rm "$DIR/$3"
	else
		GOT=$(ls -t $DIR | head -1)
		echo "'$ENAME $1' failed: expected '$3' got '$GOT'"
		OK=false
	fi
}

massTest() {
	INPUTS=()
	local -n FILES=$2
	for it in "${FILES[@]}"; do
		touch "$DIR/$it"
		INPUTS+=("$DIR/$it")
	done

	$EXE $1 ${INPUTS[@]}

	local -n EXPECT=$3
	for it in "${EXPECT[@]}"; do
		if test -f "$DIR/$it"; then
			rm "$DIR/$it"
		else
			echo "'$ENAME $1' failed"
			OK=false
			return
		fi
	done
	echo "'$ENAME $1' passed"
}

if test -d "$DIR"; then
	rm -r "$DIR"
fi
mkdir -p "$DIR"

singleTest "-N .txt" "file.tar.gz" "file.txt"
singleTest "-N ar -R STR" "file.tar.gz" "file.tSTR.gz"
singleTest "-N ar -R STR -I" "file.TAR.gz" "file.TSTR.gz"
singleTest "-N [a-z]+\\d -R STR -X" "file.0mp3d" "file.0STRd"
singleTest "-N .txt -E 1" "file.tar.gz" "file.tar.txt"
singleTest "-N .txt -E 999" "file.tar.gz" "file.txt"
singleTest "-M 3" "file.TXT" "file.txt"
singleTest "-M 4" "file.txt" "file.TXT"
singleTest "-M 5" "file.mp3" "file.3pm"

singleTest "-n blank" "file" "blank"
singleTest "-n il -r STR" "file" "fSTRe"
singleTest "-n il -r STR -i" "FILE" "FSTRE"
singleTest "-n [a-z]+\\d -r STR -x" "0big3num" "0STRnum"
singleTest "-m 3" "FILE" "file"
singleTest "-m 4" "file" "FILE"
singleTest "-m 5" "file" "elif"

singleTest "-o 1 -t 7" "filerino" "fo"
singleTest "-o 1 -t -2" "filerino" "fo"
singleTest "-o -8 -t 7" "filerino" "fo"
singleTest "-o -8 -t -2" "filerino" "fo"
singleTest "-o 1 -t 999" "filerino" "f"
singleTest "-o -999 -t -2" "filerino" "o"
singleTest "-f 2" "file" "le"
singleTest "-l 2" "file" "fi"

singleTest "-j STR -k 1" "file" "fSTRile"
singleTest "-j STR -k 999" "file" "fileSTR"
singleTest "-j STR -k -2" "file" "filSTRe"
singleTest "-j STR -k -999" "file" "STRfile"
singleTest "-p STR" "file" "STRfile"
singleTest "-s STR" "file" "fileSTR"

INAMES=("ai" "bi" "ci" "di" "ei")
ONAMES=("dec01_ai" "dec04_bi" "dec07_ci" "dec10_di" "dec13_ei")
massTest "-z -K 0 -L 1 -T 3 -B 10 -G 2 -C 0 -P dec -S _" INAMES ONAMES

if $OK; then
	rm -r $DIR
else
	exit 1
fi
