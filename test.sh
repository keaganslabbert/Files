#!/bin/bash

INPUT_DIR="tests/input"
EXPECTED_DIR="tests/expected"
LOG_DIR="tests/logs"
OUT_DIR="tests/out"
PROGRAM="schedule_processes"

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# run test.sh -g to gen expected
GENERATE=0

if [[ "$1" == "--generate" || "$1" == "-g" ]]; then
    GENERATE=1
    echo -e "${GREEN} GENERATE MODE ENABLED${NC}"
    echo " Output files will be saved to $EXPECTED_DIR"
fi

mkdir -p "$LOG_DIR" "$OUT_DIR" "$EXPECTED_DIR"

if [ ! -f "$PROGRAM" ] || [ ! -x "$PROGRAM" ]; then
    echo -e "${RED}Error: Executable $PROGRAM not found or is not executable.${NC}"
    exit 1
fi

PASSED=0
FAILED=0
TOTAL=0

# Schedulers: 0 (Priority), 1 (Round Robin), 2 (FCFS)
SCHEDULERS=(0 1 2)
SCHED_NAMES=("Priority" "Round Robin" "FCFS")

# Delete previous thr0.log and thr0.out files
if [ -f "thr0.log" ]; then
  rm thr0.log 
fi
if [ -f "thr0.out" ]; then
  rm thr0.out
fi

for input_file in "$INPUT_DIR"/*.list; do
    [ -e "$input_file" ] || { echo "No test cases found in $INPUT_DIR."; exit 0; }

    filename=$(basename -- "$input_file")
    testname="${filename%.*}"

    # Run the test case for each scheduler type
    for i in "${!SCHEDULERS[@]}"; do
        sched="${SCHEDULERS[$i]}"
        sched_name="${SCHED_NAMES[$i]}"

        $PROGRAM 1 "$input_file" "$sched" 2 > /dev/null 2>&1

        # Check if the program generated the expected output files
        if [ ! -f "thr0.log" ] || [ ! -f "thr0.out" ]; then
            printf "${RED}[FAIL]${NC} | %-20s \t [%s] (Missing thr0.log/out)\n" "$testname" "$sched_name"
            ((FAILED++))
            ((TOTAL++))
            continue
        fi

        if [ $GENERATE -eq 1 ]; then
            # Prefix with scheduler ID
            mv thr0.log "$EXPECTED_DIR/${sched}_${testname}.log.exp"
            mv thr0.out "$EXPECTED_DIR/${sched}_${testname}.out.exp"
            printf "${GREEN}[GEN ]${NC} | %-20s \t [%s]\n" "$testname" "$sched_name"
        else
            # Prefix with scheduler ID
            mv thr0.log "$LOG_DIR/${sched}_${testname}.log"
            mv thr0.out "$OUT_DIR/${sched}_${testname}.out"

            expected_log="$EXPECTED_DIR/${sched}_${testname}.log.exp"

            if [ ! -f "$expected_log" ]; then
                printf "${RED}[FAIL]${NC} | %-20s \t [%s] (Missing expected log)\n" "$testname" "$sched_name"
                ((FAILED++))
            else
                diff_file="$LOG_DIR/${sched}_${testname}.diff"
                diff -u "$expected_log" "$LOG_DIR/${sched}_${testname}.log" > "$diff_file"

                if [ $? -eq 0 ]; then
                    printf "${GREEN}[PASS]${NC} | %-20s \t [%s]\n" "$testname" "$sched_name"
                    ((PASSED++))
                    rm "$diff_file"
                else
                    printf "${RED}[FAIL]${NC} | %-20s \t [%s] (See %s)\n" "$testname" "$sched_name" "$diff_file"
                    ((FAILED++))
                fi
            fi
        fi
        ((TOTAL++))
    done
done

echo "--------------------------------------------------"
if [ $GENERATE -eq 1 ]; then
    echo -e "${GREEN}Generation complete. $TOTAL expected outputs updated.${NC}"
else
    # Color the summary based on whether any tests failed
    if [ $FAILED -eq 0 ]; then
        echo -e "${GREEN}Test Run Summary: $PASSED passed, $FAILED failed, $TOTAL total.${NC}"
    else
        echo -e "${RED}Test Run Summary: $PASSED passed, $FAILED failed, $TOTAL total.${NC}"
        exit 1
    fi
fi
