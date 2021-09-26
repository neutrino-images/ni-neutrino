#!/bin/sh

# kill start script first to ignore neutrino's exit codes
killall start_neutrino

# run shutdown routine
/etc/init.d/rcK
