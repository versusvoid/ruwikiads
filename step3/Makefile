
XGBOOST_DIR ?= ../xgboost

all: a.out

# \
	g++ --std=c++14 -g3 -ggdb3 -pthread -Wall \
	g++ --std=c++14 -O3 -pthread -Wall \


a.out: main.cpp features.hpp Makefile
	g++ --std=c++14 -O3 -pthread -Wall \
		main.cpp \
		-I${XGBOOST_DIR}/include \
		-L${XGBOOST_DIR}/lib \
		-L${XGBOOST_DIR}/dmlc-core \
		-L${XGBOOST_DIR}/rabit/lib \
		-L${XGBOOST_DIR}/lib \
		-Wl,-Bdynamic -lboost_system -lboost_filesystem \
		-Wl,-Bstatic -lxgboost -lrabit -ldmlc -Wl,-Bdynamic \
		-lgomp -lrt -lm -ldl \

run: a.out
	sh -c 'time ./a.out ${features}'

clean:
	rm a.out
