#!/usr/bin/env python3
#
# Copyright (c) 2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
This script checks for git modules
"""

import subprocess
import sys

ALLOWED_SUBMODULE_PATHS = {
    "src/crypto/randomx",
}

def main():
    submodules_list = subprocess.check_output(['git', 'submodule', 'status', '--recursive'],
                                                text = True, encoding = 'utf8').rstrip('\n')
    submodules = [line for line in submodules_list.splitlines() if line]
    unexpected_submodules = [
        line for line in submodules
        if line.split()[1] not in ALLOWED_SUBMODULE_PATHS
    ]
    if unexpected_submodules:
        print("These submodules were found, delete them:\n", submodules_list)
        sys.exit(1)
    sys.exit(0)

if __name__ == '__main__':
    main()
