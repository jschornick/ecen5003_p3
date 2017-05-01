#!/bin/bash

# ECEN5003, Project 3, Module 4
# Author: Jeff Schornick (jesc5667)

# kernel_info.sh
#
# A script to log useful information about the Linux kernel

# Since we want to run at startup, send output to system logger by default
# Provide -p parameter to send output to stdout
if [ "$1" = "-p" ]; then
    OUT=echo
else
    OUT=logger
fi

# How long to sleep between reports
DELAY=10

func_release() {
    RELEASE=$(uname -r)
    ${OUT} Kernel release: ${RELEASE}
}

# Output the kernel command line
func_kernel_cmdline() {
    CMDLINE=$(cat /proc/cmdline)
    ${OUT} Kernel command line: ${CMDLINE}
}

func_processor_type() {
    PROC_TYPE=$(awk -F: /model\ name/'{print $2; exit}' /proc/cpuinfo)
    ${OUT} Processor Type: ${PROC_TYPE}
}

# Output the number of modules loaded
func_modules() {
    MODULES=$(cat /proc/modules | wc -l)
    ${OUT} Number of loaded modules: ${MODULES}
}

# Output number of running processes
func_processes() {
    PROCESSES=$(ps --no-headers -e | wc -l)
    PROCESSES=$(ps --no-headers -e | wc -l)
    ROOT_PROCS=$(ps -U root | wc -l)
    ${OUT} Total processes: ${PROCESSES}, root: ${ROOT_PROCS}
}

# Output memory use
func_memory_use() {
    MEM_TOTAL=$(free | awk /Mem:/'{print $2}')
    MEM_USED=$(free | awk /Mem:/'{print $3}')
    ${OUT} Memory use: ${MEM_USED} / ${MEM_TOTAL} kbytes
}

# Output filesystem usage
func_fs_use() {
    FS_USE=$(df / | tail -n +2 | awk '{print $5}')
    ${OUT} Root filesytem use: ${FS_USE}
}


# Main routine

# Output these one time only
func_release
func_processor_type
func_kernel_cmdline
func_modules

# Output these continuously
while :; do
    func_processes
    func_memory_use
    func_fs_use
    sleep ${DELAY}
done
