

all: xgb-dmatrix-translator.out

# \
	g++ --std=c++14 -g3 -ggdb3 -pthread -Wall \
        g++ --std=c++14 -O3 -pthread -Wall \


xgb-dmatrix-translator.out: xgb-dmatrix-translator.cpp
	g++ --std=c++14 -O3 -pthread -Wall \
		xgb-dmatrix-translator.cpp \
		-I/home/versus/tmp/xgboost/include \
		-L/home/versus/tmp/xgboost/lib \
		-L/home/versus/tmp/xgboost/dmlc-core \
		-L/home/versus/tmp/xgboost/rabit/lib \
		-L/home/versus/tmp/xgboost/lib \
                -Wl,-Bdynamic -lboost_system -lboost_filesystem \
		-Wl,-Bstatic -lxgboost -lrabit -ldmlc -Wl,-Bdynamic \
		-lgomp -lrt -lm -ldl \
		-o xgb-dmatrix-translator.out

clean:
	rm a.out
