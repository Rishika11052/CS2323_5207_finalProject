#!/bin/bash

# --- Configuration ---
SIM_EXE="../build/vm"
TEST_DIR=$1
mkdir -p "$TEST_DIR"

# Colors
if command -v tput > /dev/null; then
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    BLUE=$(tput setaf 4)
    BOLD=$(tput bold)
    NC=$(tput sgr0)
else
    RED=""
    GREEN=""
    BLUE=""
    BOLD=""
    NC=""
fi

# --- Helper Functions ---

configure_cache() {
    local enabled=$1
    local lines=$2
    local assoc=$3
    local block=$4
    local repl=$5
    local w_miss=$6
    
    (
        echo "modify_config Cache cache_enabled $enabled"
        echo "modify_config Cache number_of_lines $lines"
        echo "modify_config Cache cache_associativity $assoc"
        echo "modify_config Cache cache_block_size $block"
        echo "modify_config Cache cache_replacement_policy $repl"
        echo "modify_config Cache cache_write_miss_policy $w_miss"
        echo "exit"
    ) | $SIM_EXE --start-vm > /dev/null 2>&1
}

get_stat() {
    local key=$1
    local output="$2"
    echo "$output" | grep "$key" | awk -F': ' '{print $2}' | tr -d ' '
}

# --- Main Execution ---

echo -e "${BOLD}Building Simulator...${NC}"
(cd .. && make > /dev/null)

echo -e "\n${BOLD}=== Cache Analysis Suite ===${NC}"

# Loop through all .s files in the cache directory
for test_file in "$TEST_DIR"/*.s; do
    [ -e "$test_file" ] || continue 
    
    filename=$(basename "$test_file")
    echo -e "\n${BLUE}${BOLD}Test File: $filename${NC}"
    
    # Print Table Header
    printf "%-30s | %-8s | %-8s | %-8s | %-8s | %-8s\n" "Configuration" "Access" "Hits" "Misses" "Evicts" "Rate"
    echo "-------------------------------|----------|----------|----------|----------|----------"
    
    # 1. Direct Mapped (Small)
    # Lines=4, Assoc=1 (1 Way), Block=4
    configure_cache true 4 1 4 LRU write_allocate
    output=$($SIM_EXE --run "$test_file" 2>&1)
    
    acc=$(get_stat "Accesses:" "$output"); hit=$(get_stat "Hits:" "$output"); miss=$(get_stat "Misses:" "$output"); evict=$(get_stat "Evictions:" "$output"); rate=$(get_stat "Hit Rate:" "$output")
    printf "%-30s | %-8s | %-8s | %-8s | %-8s | %-8s\n" "Direct Mapped (4L, 1W)" "${acc:-0}" "${hit:-0}" "${miss:-0}" "${evict:-0}" "${rate:-0%}"

    # 2. 2-Way Set Associative
    # Lines=4, Assoc=2 (2 Way), Block=4
    configure_cache true 4 2 4 LRU write_allocate
    output=$($SIM_EXE --run "$test_file" 2>&1)
    
    acc=$(get_stat "Accesses:" "$output"); hit=$(get_stat "Hits:" "$output"); miss=$(get_stat "Misses:" "$output"); evict=$(get_stat "Evictions:" "$output"); rate=$(get_stat "Hit Rate:" "$output")
    printf "%-30s | %-8s | %-8s | %-8s | %-8s | %-8s\n" "2-Way Assoc (4L, 2W)" "${acc:-0}" "${hit:-0}" "${miss:-0}" "${evict:-0}" "${rate:-0%}"

    # 3. Fully Associative
    # Lines=4, Assoc=4 (4 Way), Block=4
    configure_cache true 4 4 4 LRU write_allocate
    output=$($SIM_EXE --run "$test_file" 2>&1)
    
    acc=$(get_stat "Accesses:" "$output"); hit=$(get_stat "Hits:" "$output"); miss=$(get_stat "Misses:" "$output"); evict=$(get_stat "Evictions:" "$output"); rate=$(get_stat "Hit Rate:" "$output")
    printf "%-30s | %-8s | %-8s | %-8s | %-8s | %-8s\n" "Fully Assoc (4L, 4W)" "${acc:-0}" "${hit:-0}" "${miss:-0}" "${evict:-0}" "${rate:-0%}"

    # 4. Larger Cache (No Conflicts)
    # Lines=16, Assoc=1, Block=4
    configure_cache true 16 1 4 LRU write_allocate
    output=$($SIM_EXE --run "$test_file" 2>&1)
    
    acc=$(get_stat "Accesses:" "$output"); hit=$(get_stat "Hits:" "$output"); miss=$(get_stat "Misses:" "$output"); evict=$(get_stat "Evictions:" "$output"); rate=$(get_stat "Hit Rate:" "$output")
    printf "%-30s | %-8s | %-8s | %-8s | %-8s | %-8s\n" "Large Direct (16L, 1W)" "${acc:-0}" "${hit:-0}" "${miss:-0}" "${evict:-0}" "${rate:-0%}"

    # 5. Policy Check: No Write Allocate
    # Lines=4, Assoc=1, Block=4 (Same as #1 but policy diff)
    configure_cache true 4 1 4 LRU no_write_allocate
    output=$($SIM_EXE --run "$test_file" 2>&1)
    
    acc=$(get_stat "Accesses:" "$output"); hit=$(get_stat "Hits:" "$output"); miss=$(get_stat "Misses:" "$output"); evict=$(get_stat "Evictions:" "$output"); rate=$(get_stat "Hit Rate:" "$output")
    printf "%-30s | %-8s | %-8s | %-8s | %-8s | %-8s\n" "Direct (No Write Alloc)" "${acc:-0}" "${hit:-0}" "${miss:-0}" "${evict:-0}" "${rate:-0%}"

done

echo -e "\n${GREEN}Cache analysis completed.${NC}"