#!/bin/bash

sed -i 's/^\s*</</g;/<!--.*/d' $1
sed -i ':a;N;$!ba;s/\n//g' $1
