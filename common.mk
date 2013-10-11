MPICXX = mpicxx
AR = ar
RM = rm
ECHO = echo

Q=@

ASSERT=n
ASSERT-$(ASSERT)= -DASSERT
CXXFLAGS= -O3 -Wall -std=c++98 $(ASSERT-y)

#memory
obj-y += RayPlatform/memory/ReusableMemoryStore.o 
obj-y += RayPlatform/memory/MyAllocator.o
obj-y += RayPlatform/memory/RingAllocator.o 
obj-y += RayPlatform/memory/allocator.o
obj-y += RayPlatform/memory/DefragmentationGroup.o
obj-y += RayPlatform/memory/ChunkAllocatorWithDefragmentation.o
obj-y += RayPlatform/memory/DefragmentationLane.o
obj-y += RayPlatform/memory/DirtyBuffer.o

# routing stuff for option -route-messages
obj-y += RayPlatform/routing/ConnectionGraph.o
obj-y += RayPlatform/routing/GraphImplementation.o
obj-y += RayPlatform/routing/GraphImplementationRandom.o
obj-y += RayPlatform/routing/GraphImplementationComplete.o
obj-y += RayPlatform/routing/GraphImplementationDeBruijn.o
obj-y += RayPlatform/routing/GraphImplementationKautz.o
obj-y += RayPlatform/routing/GraphImplementationExperimental.o
obj-y += RayPlatform/routing/GraphImplementationGroup.o
obj-y += RayPlatform/routing/Polytope.o
obj-y += RayPlatform/routing/Torus.o

# communication
obj-y += RayPlatform/communication/mpi_tags.o
obj-y += RayPlatform/communication/VirtualCommunicator.o
obj-y += RayPlatform/communication/BufferedData.o
obj-y += RayPlatform/communication/Message.o
obj-y += RayPlatform/communication/MessagesHandler.o
obj-y += RayPlatform/communication/MessageQueue.o
obj-y += RayPlatform/communication/MessageRouter.o

# scheduling
obj-y += RayPlatform/scheduling/VirtualProcessor.o
obj-y += RayPlatform/scheduling/TaskCreator.o
obj-y += RayPlatform/scheduling/SwitchMan.o

# core
obj-y += RayPlatform/core/ComputeCore.o
obj-y += RayPlatform/core/MiniRank.o
obj-y += RayPlatform/core/slave_modes.o 
obj-y += RayPlatform/core/OperatingSystem.o
obj-y += RayPlatform/core/master_modes.o
obj-y += RayPlatform/core/statistics.o

# modular plugin architecture
obj-y += RayPlatform/plugins/CorePlugin.o
obj-y += RayPlatform/plugins/RegisteredPlugin.o

# structures
obj-y += RayPlatform/structures/StaticVector.o 

# profiling
obj-y += RayPlatform/profiling/Profiler.o
obj-y += RayPlatform/profiling/Derivative.o
obj-y += RayPlatform/profiling/TickLogger.o
obj-y += RayPlatform/profiling/TimePrinter.o
obj-y += RayPlatform/profiling/ProcessStatus.o

# handlers
obj-y += RayPlatform/handlers/MasterModeExecutor.o
obj-y += RayPlatform/handlers/SlaveModeExecutor.o
obj-y += RayPlatform/handlers/MessageTagExecutor.o

#cryptography
obj-y += RayPlatform/cryptography/crypto.o

#store
obj-y += RayPlatform/store/KeyValueStore.o
obj-y += RayPlatform/store/KeyValueStoreItem.o
obj-y += RayPlatform/store/KeyValueStoreRequest.o

# actor model for the win (FTW)
# Gul Agha, Massachusetts Institute of Technology, Cambridge, MA
# Actors: a model of concurrent computation in distributed systems
# http://dl.acm.org/citation.cfm?id=7929

obj-y += RayPlatform/actors/Actor.o

