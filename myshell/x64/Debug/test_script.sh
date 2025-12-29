#!/bin/bash
# Test script for shell interpreter

# Test variable assignment
name="John"
age=25

# Test echo with variables
echo "Name: $name"
echo "Age: $age"

# Test if-else statement
if [ $age -gt 18 ]; then
    echo "Adult"
else
    echo "Minor"
fi

# Test for loop
for i in 1 2 3; do
    echo "Number: $i"
done

# Test while loop
count=0
while [ $count -lt 3 ]; do
    echo "Count: $count"
    count=$((count + 1))
done

# Test case statement
case $name in
    "John")
        echo "Hello John"
        ;;
    "Jane")
        echo "Hello Jane"
        ;;
    *)
        echo "Hello stranger"
        ;;
esac

# Test command execution
pwd