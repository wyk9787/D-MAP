ROOT = .
DIRS = user shared_library tests server worker
LIBS = pthread dl

include $(ROOT)/common.mk
