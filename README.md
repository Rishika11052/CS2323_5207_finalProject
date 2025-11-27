# RISC-V 5-Stage Pipeline Simulator (Backend)

## 📖 Description
This is a cycle-accurate C++ simulator for a 5-stage RISC-V processor (IF, ID, EX, MEM, WB). It supports the standard RV64I instruction set along with the **F and D (Floating Point) extensions**. The simulator features a configurable architecture allowing runtime switching between:
* **Single-Cycle Mode**
* **5-Stage Pipeline Modes:**
    * Naive (No hazard handling)
    * Stalls Only (Data Hazard detection)
    * Forwarding (EX-EX, MEM-EX, MEM-ID paths)
    * Branch Prediction (Static & Dynamic 1-Bit)

It also includes a configurable **Cache Simulator** (Tag-store only) to analyze memory hierarchy performance.

## 🛠️ Prerequisites
* **C++ Compiler:** GCC (g++) supporting C++17 or later.
* **Build Tool:** GNU Make.
* **OS:** Linux / macOS (Windows requires WSL).

## 🚀 Build Instructions
1. Open a terminal in the project root.
2. Run the following commands:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```
   This will compile the source code and generate the executable at `build/vm`.

## ⚙️ Configuration
The simulator behavior is controlled by `vm_state/config.ini`. 
This file and the `vm_state` directory appears only after running the `vm` executable atleast once.

Do that by doing this inside the build folder:

```bash
./vm --start-vm
```
Followed by a ^C (Ctrl + C) to escape.

You can edit this file manually or control it via the CLI/GUI.
* **processor_type:** `single_stage` or `multi_stage`
* **hazard_detection:** `true`/`false`
* **forwarding:** `true`/`false`
* **branch_prediction:** `none`, `static`, or `dynamic_1bit`

## 💻 Usage (CLI)
To run an assembly program: (in project root)
```bash
./build/vm --run path/to/file.s
```

To assemble a program without running:
```bash
./build/vm --assemble path/to/file.s
```

## ✅ Verification & Testing
We provide an automated test suite to verify correctness and measure performance. Make sure you have build the project atleast once.

**1. Run the Automated Test Suite:**
This script runs all test cases across all 5 pipeline modes, verifies register states against the single-cycle model, and generates a performance table.
```bash
cd verification
./test.sh <Name of Test Suite(Can be any Random Thing)> <Path to Folder containing .s files>
```

**2. Run a Single Verification:**
To verify a specific file and see detailed mismatches:
```bash
./build/vm --verify verification/hazards/hz_load_use.s
```

**3. Run a Automated script for cache Testing:**
To verify all files in a specific folder and see detailed cache statistics:
```bash
cd verification
./test_cache.sh <Path of folder with .s files to test cache>
```

See [Commands](COMMANDS.md) for a list of commands to run in Interactive mode.


## References
- [Original Repo](https://github.com/VishankSingh/riscv-simulator-2)
- [RISC-V Specifications](https://riscv.org/specifications/)
- [Five EmbedDev ISA manual](https://five-embeddev.com/riscv-isa-manual/)