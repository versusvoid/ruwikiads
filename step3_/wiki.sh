#!/bin/bash

VOCAB_FILE=data/temp/vocab.txt
COOCCURRENCE_FILE=data/temp/cooccurrence.bin
COOCCURRENCE_SHUF_FILE=data/temp/cooccurrence.shuf.bin
BUILDDIR=GloVe/build
VERBOSE=2
MEMORY=10.0
MAX_VOCAB=25000
VOCAB_MIN_COUNT=23
VECTOR_SIZE=20
MAX_ITER=30
WINDOW_SIZE=15
BINARY=0
NUM_THREADS=8
X_MAX=100
ETA=0.04
SAVE_FILE=data/temp/vectors-${VECTOR_SIZE}d-${MAX_ITER}i-${WINDOW_SIZE}w

./wiki-reader.out | $BUILDDIR/vocab_count -min-count $VOCAB_MIN_COUNT -max-vocab $MAX_VOCAB -verbose $VERBOSE > $VOCAB_FILE
if [[ $? -ne 0 ]]; then
    echo "vocab_count failed"
    exit 1;
fi
echo vocab_count done

./wiki-reader.out | $BUILDDIR/cooccur -memory $MEMORY -vocab-file $VOCAB_FILE -verbose $VERBOSE -window-size $WINDOW_SIZE > $COOCCURRENCE_FILE
if [[ $? -ne 0 ]]; then
    echo "cooccur failed"
    exit 1;
fi
echo cooccur done

$BUILDDIR/shuffle -memory $MEMORY -verbose $VERBOSE < $COOCCURRENCE_FILE > $COOCCURRENCE_SHUF_FILE
if [[ $? -ne 0 ]]; then
    echo "shuffle failed"
    exit 1;
fi
echo shuffle done

$BUILDDIR/glove -save-file $SAVE_FILE -threads $NUM_THREADS -input-file $COOCCURRENCE_SHUF_FILE \
    -x-max $X_MAX -iter $MAX_ITER -eta $ETA \
    -vector-size $VECTOR_SIZE -binary $BINARY -vocab-file $VOCAB_FILE -verbose $VERBOSE
if [[ $? -ne 0 ]]; then
    echo "glove failed"
    exit 1;
fi
echo glove done

#if [ "$1" = 'octave' ]; then
#   octave < ./GloVe/eval/octave/read_and_evaluate_octave.m 1>&2 
#else
#   python ./GloVe/eval/python/evaluate.py
#fi
