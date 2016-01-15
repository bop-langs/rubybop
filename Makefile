BOP_DIR = cbop
RUBY_DIR = ruby
RB_MAKE_FILE = $(RUBY_DIR)/Makefile

default: ruby

bop_library:
	$(MAKE) -C cbop

$(RB_MAKE_FILE): $(RUBY_DIR)/Makefile.in
	@echo Configuring Ruby...
	@cd ruby && autoconf && ./configure --disable-install-doc

ruby: bop_library $(RB_MAKE_FILE)
	@echo 'Building Ruby'
	$(MAKE) -C $(RUBY_DIR)

clean: $(RB_MAKE_FILE)
	$(MAKE) -C $(BOP_DIR) clean
	$(MAKE) -C $(RUBY_DIR) clean

test: ruby
	@echo 'Testing Ruby'
	@cd $(RUBY_DIR) && ./miniruby ./bootstraptest/runner.rb
