##
## Makefile for Random Sentence Generator
##

CPPFLAGS = -g -Wall

CXX = g++
LDFLAGS = 

CLASS = random.cc production.cc definition.cc
CLASS_H = $(SRCS:.cc=.h)
SRCS = rsg.cc $(CLASS)
OBJS = $(SRCS:.cc=.o)
PROGS = rsg

default : $(PROGS) 

$(PROGS) : depend $(OBJS)
	$(CXX) -o $@ $(OBJS)   $(LDFLAGS) 

depend:: Makefile.dependencies $(SRCS) $(HDRS)

Makefile.dependencies:: $(SRCS) $(HDRS)
	$(CXX) $(CPPFLAGS) -MM $(SRCS) > Makefile.dependencies

-include Makefile.dependencies

clean : 
	/bin/rm -f *.o a.out core $(PROGS) Makefile.dependencies

TAGS : $(SRCS) $(HDRS)
	etags -t $(SRCS) $(HDRS)
