CXX		= g++ -std=c++20
INCLUDES	= -I. -I./miniz -I./include
CXXFLAGS  	+= -Wall 

LDFLAGS 	= -pthread -fopenmp
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= minizseq minizparallel

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

minizseq	: minizseq.cpp ./include/cmdline.hpp ./include/utility.hpp

minizparallel : minizparallel.cpp ./include/cmdline.hpp ./include/utility.hpp ./include/utilitypar.hpp

clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~



