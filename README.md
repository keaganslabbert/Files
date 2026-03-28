# CS314 Concurrency Project 1 Test Cases and Test Runner

Here are my test cases and test runner script for CS314 project 1.

## DISCLAIMER:
I cannot guarantee my outputs are correct, but I tried my best to follow the 
spec as closely as I could. And my outputs match the given test case output
on STEMLearn.
Also there are no parallel cases/outputs due to non-determism.

## NOTE:
I suggest adding `*.diff` to your .gitignore to make sure your repo remains
clean.

## Usage:
First run `make` to compile your program.

Then run the test script with `./test.sh` in the root directory of your repo.
(You might need to give the script execute permissions with `chmod +x test.sh`)

If you would like to add your own test cases and generate expected outputs for 
them you can run `./test.sh -g` 
(this will overwrite my expected outputs though).

## Directory Structure
This is the expected directory structure that the test script uses:

```
.
├── Makefile
├── README.md
├── schedule_processes
├── src
├── test.sh
└── tests
    ├── README.md
    ├── expected
    │   ├── 0_00_process1.log
    │   ├── 0_00_process1.out
    │   ├── ...
    │   ├── 1_00_process1.log
    │   ├── 1_00_process1.out
    │   ├── ...
    │   ├── 2_00_process1.log
    │   ├── 2_00_process1.out
    │   └── ...
    ├── input
    │   ├── 00_process1.list
    │   └── ...
    ├── logs
    │   ├── 0_00_process1.log
    │   ├── ...
    │   ├── 1_00_process1.log
    │   ├── ...
    │   ├── 2_00_process1.log
    │   └── ...
    └── out
        ├── 0_00_process1.out
        ├── ...
        ├── 1_00_process1.out
        ├── ...
        ├── 2_00_process1.out
        └── ...
```

The priority scheduler outputs start with `0_`, round-robin with `1_` and FCFS 
with `2_`.
I also included my `.out` files so you can compare them with your own when 
debugging or checking my answers.

---

# Updates:
23-03-2026 - Added testcases for unknown instructions and when there are 
instruction lists for processes that aren't in the process list.
Also fixed output for the testcase on STEMLearn (sorry!) 
Thank you Y. A. Boye and Yasr Jappie.
23-03-2026 - Fixed round-robin expected output (sorry!) Thank you 🥷🏿.
27-03-2026 - Fixed output for 12,13,16. Thanks Jared.

# Credits:
- Michael Swartz (Demi and 🐐)
He shared his test cases from last year with me which you can check out at
`https://github.com/MichaelBruwerSwartz/SchedulerTester`
Make sure to star his repo so he can get the achievement. (Take note they seem
to be outdated and the expected outputs dont exactly match this year's due to 
small changes in the spec)
- Gemini (lol)
Used Gemini to help write the test runner script, but the test cases I wrote by 
hand I promise.
- Everyone that pointed out errors

If you disagree and would like to correct me on any of my outputs feel free
to DM me on WhatsApp or @me on the CS314 group (I'm Charl on the group).

Also my number is 074 330 0120 and email is 22621318@sun.ac.za

Have fun and good luck!

P.S. if you would like to share some cases to be added, I'll gladly accept them 
and credit you too (even if they are made with AI haha). :)
