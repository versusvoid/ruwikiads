
XGBOOST_DIR ?= ../xgboost

all: xgb-dmatrix-translator.out wiki-reader.out

# \
	g++ --std=c++14 -g3 -ggdb3 -pthread -Wall \
	g++ --std=c++14 -O3 -pthread -Wall \

wiki-reader.out: wiki-reader.cpp
	g++ --std=c++14 -O3 -Wall wiki-reader.cpp -o wiki-reader.out

xgb-dmatrix-translator.out: xgb-dmatrix-translator.cpp
	g++ --std=c++14 -O3 -pthread -Wall \
		xgb-dmatrix-translator.cpp \
		-I${XGBOOST_DIR}/include \
		-L${XGBOOST_DIR}/lib \
		-L${XGBOOST_DIR}/dmlc-core \
		-L${XGBOOST_DIR}/rabit/lib \
		-L${XGBOOST_DIR}/lib \
        -Wl,-Bdynamic -lboost_system -lboost_filesystem \
		-Wl,-Bstatic -lxgboost -lrabit -ldmlc -Wl,-Bdynamic \
		-lgomp -lrt -lm -ldl \
		-o xgb-dmatrix-translator.out

run: xgb-dmatrix-translator.out data/temp/vectors-20d-30i-15w.txt
	sh -c 'time ./xgb-dmatrix-translator.out'

data/temp/vectors-20d-30i-15w.txt: wiki-reader.out ../step1/data/output/ads-samples.stemmed.txt.bz2 ../step2/data/output/ads.stemmed.txt.bz2
	./wiki.sh
	#echo wtf

vectors: data/temp/vectors-20d-30i-15w.txt


clean:
	rm a.out
