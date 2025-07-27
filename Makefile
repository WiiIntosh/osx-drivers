.PHONY: all clean

all:
	@$(MAKE) -C WiiPlatform all
	@$(MAKE) -C WiiGraphics all
	@$(MAKE) -C WiiStorage all
	@$(MAKE) -C WiiUSB all

clean:
	@$(MAKE) -C WiiPlatform clean
	@$(MAKE) -C WiiGraphics clean
	@$(MAKE) -C WiiStorage clean
	@$(MAKE) -C WiiUSB clean
