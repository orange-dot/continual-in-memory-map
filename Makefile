ISABELLE ?= isabelle
ISABELLE_BUILD_OPTIONS ?= -S
ISABELLE_COMMAND_TIMEOUT ?= 60
ISABELLE_USER_HOME ?= $(CURDIR)/.isabelle-user
ISABELLE_SETTINGS := $(ISABELLE_USER_HOME)/.isabelle/etc/settings

.PHONY: proof proof-build clean-proof

proof: $(ISABELLE_SETTINGS) scripts/isabelle-proof.sh
	ISABELLE=$(ISABELLE) ISABELLE_USER_HOME=$(ISABELLE_USER_HOME) ISABELLE_COMMAND_TIMEOUT=$(ISABELLE_COMMAND_TIMEOUT) scripts/isabelle-proof.sh

proof-build: $(ISABELLE_SETTINGS)
	USER_HOME=$(ISABELLE_USER_HOME) $(ISABELLE) build $(ISABELLE_BUILD_OPTIONS) -d docs/isabelle CIM_V0a

$(ISABELLE_SETTINGS): docs/isabelle/settings
	mkdir -p $(dir $@)
	cp $< $@

clean-proof:
	rm -rf $(ISABELLE_USER_HOME)
