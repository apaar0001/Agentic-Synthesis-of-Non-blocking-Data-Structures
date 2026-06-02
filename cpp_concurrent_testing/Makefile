CXX = clang++
CXX_FLAGS = -std=c++17 -O3 -pthread

TEST_DIR = test
STRUCT_DIR = structures
BIN_DIR = bin

TESTS = BenchmarkSequential NonBlockingTest ConsistencyTest LockFreedomTest QueueNonBlockingTest StackNonBlockingTest QueueConsistencyTest StackConsistencyTest

all: $(TESTS)

BenchmarkSequential: $(TEST_DIR)/BenchmarkSequential.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperations.hpp $(STRUCT_DIR)/LockFreeList.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

NonBlockingTest: $(TEST_DIR)/NonBlockingTest.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperations.hpp $(STRUCT_DIR)/LockFreeList.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

ConsistencyTest: $(TEST_DIR)/ConsistencyTest.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperations.hpp $(STRUCT_DIR)/LockFreeList.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

LockFreedomTest: $(TEST_DIR)/LockFreedomTest.cpp $(TEST_DIR)/TestConfig.hpp $(STRUCT_DIR)/LockFreeList.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

QueueNonBlockingTest: $(TEST_DIR)/QueueNonBlockingTest.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperationsQueue.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

StackNonBlockingTest: $(TEST_DIR)/StackNonBlockingTest.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperationsStack.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

QueueConsistencyTest: $(TEST_DIR)/QueueConsistencyTest.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperationsQueue.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

StackConsistencyTest: $(TEST_DIR)/StackConsistencyTest.cpp $(TEST_DIR)/TestConfig.hpp $(TEST_DIR)/RunOperationsStack.hpp
	$(CXX) $(CXX_FLAGS) -o $(BIN_DIR)/$@ $<

clean:
	rm -rf $(BIN_DIR)/*
