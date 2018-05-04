ROOT = .
DIRS = tests src/user src/shared_library src/server src/worker
LIBS = pthread dl

include $(ROOT)/common.mk
