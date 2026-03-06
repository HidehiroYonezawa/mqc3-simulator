.PHONY: all clean TARGETS

define TARGETS
	../proto/mqc3_cloud/program/v1/circuit.proto
	../proto/mqc3_cloud/program/v1/graph.proto
	../proto/mqc3_cloud/scheduler/v1/execution.proto
endef

all: $(TARGETS:.proto=.cpp)

%.cpp: %.proto
	buf generate $< --include-imports

clean:
	rm -rf mqc3_cloud
