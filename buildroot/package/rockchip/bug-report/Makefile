PROJECT_DIR := $(shell pwd)
PROM	= bug-report
#CXX	?= ../../../output/host/usr/bin/aarch64-rockchip-linux-gnu-g++

CXXFLAGS ?= -fPIC -O3 -I$(PROJECT_DIR) -lpthread
OBJ = bug_report.o  main.o

$(PROM): $(OBJ)
	$(CXX) -o $(PROM) $(OBJ) $(CXXFLAGS)
%.o: %.c
	$(CXX) -c $< -o $@ $(CXXFLAGS)
clean:
	@rm -rf $(OBJ) $(PROM)

install:
	sudo install -D -m 755 bug-report -t /usr/bin/
