#!/bin/bash

if [ $# -lt 2 ]
then
	echo "ERROR: too few arguments"
	echo "Usage: <output file> <latency file>"
	exit 1
fi

FILE="${1}.tgt"
LATENCY="${2}"
ENCODER_LIST="soda09_16 hybrid-16"

rm -f ${FILE}

for ENC in ${ENCODER_LIST}
do
	echo "== ${ENC}" | tee -a ${FILE}
	taskset 0x1 ./calibrator ${LATENCY} ${ENC} | tee -a ${FILE}
	echo "" >> ${FILE}
done
