# PandOS - Operating System Project

## I. Overview

**PandOS** is an educational operation system developed as part of the CS372 - Operating Systems course (taken in Spring '25), designed to illustrate fundamental concepts in operating system design and implementation. 
The operating system runs on the µMPS3 emulator, a MIPS-based architecture simulator specfically developed for educational purposes.

### Project Goals

* Understand and implement key OS functionalities, including:

  * Process scheduling (Round-Robin algorithm)
  * Memory management (virtual memory and TLB handling)
  * Exception and interrupt handling
  * Device I/O operations
  * System call implementation (SYS1-SYS18)
  
* Gain hands-on experience with kernel-level programming and debugging.

## II. Project Structure

The project is organized into five phases, corresponding to the coursework assignments. The repository is structured as follows:
```
CS372-Operating-System/
├── h/                       # Header files (constants, types, and function prototypes)
├── phase1/                  # Queue Manager: PCB allocation/deallocation, queue and semaphore management
│   ├── asl.c                # Active Semaphore List management
│   ├── pcb.c                # Process Control Block management
├── phase2/                  # Nucleus: Process scheduling, exception/interrupt handling, basic syscalls
│   ├── initial.c            # Kernel initialization
│   ├── scheduler.c          # Process scheduling
│   ├── exceptions.c         # Exception handling
│   ├── interrupts.c         # Interrupt handling
├── phase3/                  # Support Level: Virtual memory (paging), TLB refill, advanced syscall handling
│   ├── initProc.c           # Initialization for user-level processes
│   ├── sysSupport.c         # System call and exception handling for support level
├── phase4/                  # DMA Device Support: Disk and flash I/O operations, DMA buffer management
│   ├── deviceSupportDMA.c   # DMA device operations
├── phase5/                  # Delay Facility: Implements timed suspension for user processes
│   ├── delayDaemon.c        # Delay daemon and delay list management
├── testers/                 # Test programs for validating user-processes (from phase 3 and beyond)
├── README.md                # Project documentation
├── .gitignore               # Git ignore file
```
Each subsequent phase builds upon previous phases, with Phase 1-3 forming the PandOS core, while Phase 4-5 add advanced features.

## III. Detailed Phase Description

### Phase 1: Queue Manager

* Allocation/deallocation of process control blocks (PCBs)
* Maintenance of process queues and process trees
* Management of active semaphore lists (ASL)

### Phase 2: Nucleus

* Kernel initialization and basic scheduling (Round-Robin with a 5ms quantum)
* Handling of device interrupts and exceptions
* Implementation of basic syscalls (SYS1-SYS8)

### Phase 3: Support Level

* Virtual memory management (paging, TLB refill)
* Advanced syscall handling, including TLB exceptions and Program Traps
* Enhanced exception processing and address translation

### Phase 4: DMA Device Support

* Implementation of DMA buffers
* Disk and flash device I/O operations
* Enhanced backing store management

### Phase 5: Delay Facility

* Time suspension facility for user processes via the delay daemon
* Management of active delay list (ADL)

## IV. Testing 

Each phase includes dedicated testing program(s) to verify functionality and robustness. For Phase 1 and Phase 2, the test file is located within the respective phase directories. For Phase 3 and beyond, several testers are available in the `tester\` directory, providing comprehensive diagnostics and validation. These testers can be utilized by loading them into the flash devices within the µMPS3 emulator.

## V. Setup Instructions

### 1. Prerequisites

* [µMPS3 Emulator](https://github.com/virtualsquare/umps3): A MIPS-based emulator for OS development
* GCC cross-compiler for MIPS (`mipsel-linux-gnu-gcc`)
* Standard Linux environment (recommended Ubuntu or WSL)

### 2. Building and Running PandOS

1. **Compile**:

```bash
make
```

2. **Run in µMPS3**:

* Launch µMPS3 GUI.

```bash
umps3 &
```

* Create and configure a new machine with the following recommended settings:

  * TLB Floor Address: `0x8000.0000` or higher
  * RAM Size (Frames): at least 128 frames

* Device Setup:
  * Load and enable disk devices (`disk0`, `disk1`)
  * Load and enable flash devices corresponding to the desired `UPROCMAX` (defined in `\h\constants.c`)
  * Load and enable necessary printers and terminals
  
* Run the emulator.

## V. Authors

**Uyen Nguyen** - Applied Math and Computer Science, Denison University (Class of 2026)

For questions, improvements, or issues, please contact via [nguyen_u1@denison.edu](nguyen_u1@denison.edu) or GitHub.
