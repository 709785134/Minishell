#!/bin/bash
# test_case.sh
name="John"

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