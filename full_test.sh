#!/bin/bash

make test1
./sfs

echo ""
echo ""
echo "###################################"
echo "## Finished running test1          "
echo "## Pausing 2s then continuing      "
echo "###################################"
echo ""
echo ""

sleep 2

make test2
./sfs
