#!/usr/bin/env bash

# To be used in test system
RUN_PREFIX=""

START=$(date +"%s.%N")
$RUN_PREFIX /opt/test_bins/regex_matcher /opt/test_data/re /opt/test_data/text /opt/test_data/out
END=$(date +"%s.%N")
DURATION=$(echo "$END - $START" | bc -l)

if ! diff /opt/test_data/out /opt/test_answ/out ; then
    echo """
    {
        "code": "WA",
        "time": "$DURATION"
    }"""
else
    echo """
    {
        "code": "OK",
        "time": "$DURATION"
    }"""
fi
