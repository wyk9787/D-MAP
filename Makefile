ROOT = .
DIRS = shared_library client
TARGETS = worker server
LIBS = pthread dl

include $(ROOT)/common.mk
