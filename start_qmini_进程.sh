#! /bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:install/x86_64/lib/
./install/x86_64/bin/run_interface &> qmini_log.log &

