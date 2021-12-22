#!/bin/bash

sed -i 's/^\s*</</g;/<!--.*/d' $1
