BASE_DIR=/home/prakash/software/z3
#BASE_DIR=/home/pmurali/software/z3

#removed DNDEBUG flag
FLAGS = -D_MP_INTERNAL -D_AMD64_ -D_USE_THREAD_LOCAL -D_EXTERNAL_RELEASE  -std=c++11 -fvisibility=hidden -c -mfpmath=sse -msse -msse2 -fopenmp -O3 -D _EXTERNAL_RELEASE -fomit-frame-pointer -D_LINUX_ -fPIC -D_LINUX_ -g

opts: ejf.o read.o machine.o expt.o schedule.o trials.o utils.o output.o lazy.o
	g++ $(FLAGS) -o ejf.o  -I$(BASE_DIR)/src/api ejf.cpp
	g++ $(FLAGS) -o algo.o  -I$(BASE_DIR)/src/api algo.cpp
	g++ $(FLAGS) -o expt.o  -I$(BASE_DIR)/src/api expt.cpp
	g++ $(FLAGS) -o read.o  -I$(BASE_DIR)/src/api read.cpp
	g++ $(FLAGS) -o output.o  -I$(BASE_DIR)/src/api output.cpp
	g++ $(FLAGS) -o utils.o  -I$(BASE_DIR)/src/api utils.cpp
	g++ $(FLAGS) -o machine.o  -I$(BASE_DIR)/src/api machine.cpp
	g++ $(FLAGS) -o schedule.o  -I$(BASE_DIR)/src/api schedule.cpp
	g++ $(FLAGS) -o trials.o  -I$(BASE_DIR)/src/api trials.cpp
	g++ $(FLAGS) -o two_stage.o  -I$(BASE_DIR)/src/api two_stage.cpp
	g++ $(FLAGS) -o ms_sr.o  -I$(BASE_DIR)/src/api ms_sr.cpp
	g++ $(FLAGS) -o lazy.o  -I$(BASE_DIR)/src/api lazy.cpp
	g++ -o opti ejf.o algo.o read.o output.o utils.o machine.o expt.o schedule.o two_stage.o trials.o lazy.o ms_sr.o -lz3 -lpthread  -fopenmp -lrt

