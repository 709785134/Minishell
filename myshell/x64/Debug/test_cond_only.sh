#!/bin/bash
count=0
echo "Before condition check: $count"
# Check the condition manually
if [ $count -lt 3 ]; then
    echo "Condition is true"
else
    echo "Condition is false"
fi
count=$((count + 1))
echo "After increment: $count"