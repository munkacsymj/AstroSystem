OBJECTS = summarize_sessions.o
TARGETS = summarize_sessions  
all: $(TARGETS)

summarize_sessions: summarize_sessions.o
	$(CXXLD) -g summarize_sessions.o $(LIB_DIR) $(ALL_LIBS) -o summarize_sessions
	ln -s -f $(PWD)/summarize_sessions $(BIN_DIR)/summarize_sessions

include ../astro.prog.mk
