#!/bin/bash

# --- Configuration ---
SIM_EXE="../build/vm"
DUMP_FILE="../build/vm_state/vm_state_dump.json"
TEST_DIR=$2
SUITE_NAME=$1

# Colors
if command -v tput > /dev/null; then
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    BLUE=$(tput setaf 4)
    BOLD=$(tput bold)
    NC=$(tput sgr0) # No Color / Reset
else
    # Fallback if tput is missing
    RED=""
    GREEN=""
    BLUE=""
    BOLD=""
    NC=""
fi

# --- Helper Functions ---

# Configure the simulator by piping commands to it
# Usage: configure_mode <proc_type> <hazard> <forwarding> <prediction>
configure_mode() {
    local proc=$1
    local hazard=$2
    local fwd=$3
    local pred=$4

    echo -e "${BLUE}Configuring Simulator: $proc, Hazard=$hazard, Fwd=$fwd, Pred=$pred${NC}"
    
    # We pipe these commands to the simulator. 
    # It updates config.ini and exits.
    (
        echo "modify_config Execution processor_type $proc"
        echo "modify_config Execution hazard_detection $hazard"
        echo "modify_config Execution forwarding $fwd"
        echo "modify_config Execution branch_prediction $pred"
        echo "exit"
    ) | $SIM_EXE --start-vm > /dev/null 2>&1
}

# Run a suite of tests
run_suite() {
    local suite_name=$1
    local suite_dir=$2
    
    echo -e "\n${BOLD}=== Test Suite: $suite_name ===${NC}"
    
    # Header format: Increased CPI width to 8
    printf "%-25s | %-8s | %-8s | %-8s | %-8s | %-6s\n" "Filename" "Status" "Cycles" "Stalls" "CPI" "Misses"
    echo "--------------------------|----------|----------|----------|----------|-------"

    for test_file in "$suite_dir"/*.s; do
        [ -e "$test_file" ] || continue 
        
        filename=$(basename "$test_file")
        
        # Run Verification
        output=$($SIM_EXE --verify "$test_file" 2>&1)
        
        # Determine Pass/Fail
        if echo "$output" | grep -q "Verification PASSED"; then
            status="${GREEN}PASS${NC}"
        else
            status="${RED}FAIL${NC}"
        fi

        # Parse Metrics
        cycles=$(echo "$output" | grep "Total Cycles:" | awk '{print $3}')
        stalls=$(echo "$output" | grep "Stall Cycles:" | awk '{print $3}')
        cpi=$(echo "$output" | grep "Cycles Per Instruction (CPI):" | awk '{print $5}')
        misses=$(echo "$output" | grep "Branch Mispredictions:" | awk '{print $3}')

        # Default to 0
        cycles=${cycles:-0}
        stalls=${stalls:-0}
        cpi=${cpi:-0}
        misses=${misses:-0}

        # Print Row
        printf "%-25s | " "$filename"
        echo -ne "$status"
        
        # Increased CPI column width to %-8s
        printf "%4s | %-8s | %-8s | %-8s | %-6s\n" "" "$cycles" "$stalls" "$cpi" "$misses"
    done
}

# --- Main Execution Loop ---

# Define Modes (Arrays of config values)
# Format: Type Hazard Forwarding Prediction
declare -A modes
modes["NAIVE"]="multi_stage false false none"
modes["STALL"]="multi_stage true false none"
modes["FORWARDING"]="multi_stage true true none"
modes["STATIC"]="multi_stage true true static"
modes["DYNAMIC"]="multi_stage true true dynamic_1bit"

# Order of execution
mode_order=("NAIVE" "STALL" "FORWARDING" "STATIC" "DYNAMIC")

# 1. Build
echo -e "${BOLD}Building Simulator...${NC}"
(cd ../build && make > /dev/null)

# 2. Run Suites per Mode
for mode_name in "${mode_order[@]}"; do
    echo -e "\n\n${BOLD}████████████████████████████████████████${NC}"
    echo -e "${BOLD}      MODE: $mode_name      ${NC}"
    echo -e "${BOLD}████████████████████████████████████████${NC}"

    # Configure
    set -- ${modes[$mode_name]} # Unpack string into $1, $2, $3, $4
    configure_mode $1 $2 $3 $4

    # Run Suite    
    run_suite "$SUITE_NAME" "$TEST_DIR"
done

echo -e "\n${GREEN}All tests completed.${NC}"