#!/bin/bash

for i in {1..21} ; do ssh node-$i $@ ; done
