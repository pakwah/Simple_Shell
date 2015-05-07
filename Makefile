CXX      := g++
CXXFLAGS := -g -Wall -Wno-unused-parameter -Wextra -pedantic-errors -Werror 
CPPFLAGS := -std=c++0x

target   := main
sources  := $(wildcard *.cpp)
objects  := $(sources:.cpp=.o)
depends  := $(sources:.cpp=.dep)

$(target): $(objects)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -lpthread -lreadline -o $@

.PHONY: docs clean-docs clean-deps clean realclean

docs:
	doxygen doxygen.conf

clean-docs:
	$(RM) -r releasedDocs
	$(RM) -r docs

clean-deps:
	$(RM) $(depends)

clean: clean-deps
	$(RM) $(objects) *~ *.tmp

realclean: clean clean-docs
	$(RM) $(target)

%.dep: %.cpp
	@set -e; \
	$(CXX) -MM $(CPPFLAGS) $< > $@.tmp; \
	(echo -n "$@ " | cat - $@.tmp) > $@; \
	$(RM) $@.tmp; \
	echo -n "Generating deps [$<]:\n ";\
	cat $@


%.h %.cpp: ;
