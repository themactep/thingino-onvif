#!/bin/bash

# Quick script to add NULL conditions to dispatch table entries that don't have them

sed -i 's/}, NULL},/}, NULL},/g' src/onvif_dispatch.c
sed -i 's/},$/}, NULL},/g' src/onvif_dispatch.c
sed -i 's/{NULL, NULL, NULL}$/    {NULL, NULL, NULL, NULL}/g' src/onvif_dispatch.c

echo "Fixed dispatch table entries"
