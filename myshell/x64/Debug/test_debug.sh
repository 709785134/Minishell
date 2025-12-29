#!/bin/bash
count=0
echo "Initial count: $count"
echo "Evaluating count + 1..."
result=$((count + 1))
echo "Result: $result"
count=$result
echo "Updated count: $count"