#!/bin/bash
# Simple test script for shell interpreter

# Test variable assignment
name="John"
age=25

# Test echo with variables
echo "Name: $name"
echo "Age: $age"

# Simple arithmetic test
count=0
echo "Initial count: $count"

# Simple increment
count=$((count + 1))
echo "After increment: $count"

# Test for loop (this should work)
for i in 1 2 3; do
    echo "Number: $i"
done

# Simple pwd command
echo "Current directory:"
pwd

echo "Test completed successfully."