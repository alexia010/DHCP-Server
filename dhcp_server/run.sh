#!/bin/bash

NUM_INSTANCES=5

for ((i=1; i<=NUM_INSTANCES; i++))
do
    ./client & 
done

wait
