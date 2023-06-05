# Copyright 2020 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
# TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
# CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

#############################################################################################################
# The following flow assumes A10_OPENCL_AFU directory doesn't exist and sample design hasn't been copied over
# Arria 10 Devstack version 1.2.1
# **Adjust commands to your own needs.**
#############################################################################################################

# Initial Setup
source /data/intel_fpga/devcloudLoginToolSetup.sh
tools_setup -t A10DS

cd ~/eece-6540-labs/Labs/lab1/matrix_multi

# Check Arria 10 PAC card connectivity
aocl diagnose
error_check

# Running project in Emulation mode
printf "\\n%s\\n" "Running in Emulation Mode:"
aoc -march=emulator -v device/matrix_multi.cl -o bin/matrix_multi_emulation.aocx
# Creating symbolic link to emulation .aocx
ln -sf matrix_multi_emulation.aocx bin/matrix_multi.aocx
make
# Run host code for version 1.2.1
./bin/host -emulator
error_check

# Running project in FPGA Hardware Mode (this takes approximately 1 hour)
printf "\\n%s\\n" "Running in FPGA Hardware Mode:"
aoc device/matrix_multi.cl -o bin/matrix_multi_fpga.aocx -board=pac_a10
# Relink to hardware .aocx
ln -sf matrix_multi_fpga.aocx bin/matrix_multi.aocx
# Availavility of Acceleration cards
aoc -list-boards
error_check
# Get device name
aocl diagnose
error_check

# Converting to an unsigned .aocx file
cd bin
printf "\\n%s\\n" "Converting to unsigned .aocx:"
printf "Y\\nY\\n" | source $AOCL_BOARD_PACKAGE_ROOT/linux64/libexec/sign_aocx.sh -H openssl_manager -i matrix_multi_fpga.aocx -r NULL -k NULL -o matrix_multi_fpga_unsigned.aocx
error_check
# Programmming PAC Card
aocl program acl0 matrix_multi_fpga_unsigned.aocx
./host
error_check
