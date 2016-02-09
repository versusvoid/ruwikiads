

all: a.out

a.out: main.cpp features.hpp
	g++ --std=c++14 -O3 -pthread -Wall \
		main.cpp \
		-I/home/versus/tmp/xgboost/include \
		-L/home/versus/tmp/xgboost/lib \
		-L/home/versus/tmp/xgboost/dmlc-core \
		-L/home/versus/tmp/xgboost/rabit/lib \
		-L/home/versus/tmp/xgboost/lib \
		-Wl,-Bdynamic -lbz2 -lboost_regex -lboost_program_options -lboost_system -lboost_filesystem \
		-Wl,-Bstatic -lxgboost -lrabit -ldmlc -Wl,-Bdynamic \
		-lgomp -lrt -lm -ldl
