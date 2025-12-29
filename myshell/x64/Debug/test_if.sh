#!/bin/bash
# test_if.sh
age=25

if [ $age -gt 18 ]; then
    echo "Adult"
else
    echo "Minor"
fi