CXX		= g++ -std=c++20
INCLUDES	= -I . -I miniz
CXXFLAGS  	+= -Wall 

LDFLAGS 	= -pthread -fopenmp
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= minizseq minizparallel

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

minizseq	: minizseq.cpp cmdline.hpp utility.hpp

minizparallel : minizparallel.cpp cmdline.hpp utility.hpp utilitypar.hpp

clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~



