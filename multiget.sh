#!/bin/bash

# Do a load test querying the running server for a bunch of large 
# files concurrently.

for num in {1..10}
do
	curl localhost:8000/Assignment02.zip &
done