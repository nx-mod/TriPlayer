.PHONY: all clean

#---------------------------------------------------------------------------------
# TriPlayer version
#---------------------------------------------------------------------------------
export VER_MAJOR	:= 1
export VER_MINOR	:= 2
export VER_MICRO	:= 1
#---------------------------------------------------------------------------------

all:
	@echo -e '\033[1m>> Common (minIni)\033[0m'
	@$(MAKE) -s -C Common/libs/minIni
	@echo -e '\033[1m>> Common (Splash)\033[0m'
	@$(MAKE) -s -C Common/libs/splash
	@echo -e '\033[1m>> sqlite-nx\033[0m'
	@$(MAKE) -s -C ../sqlite-nx
	@echo -e '\033[1m>> Application\033[0m'
	@$(MAKE) -s -C Application/
	@echo -e '\033[1m>> Overlay\033[0m'
	@$(MAKE) -s -C Overlay/
	@echo -e '\033[1m>> Sysmodule\033[0m'
	@$(MAKE) -s -C Sysmodule/
	@echo -e '\033[1m>> SD Card\033[0m'
	@mkdir -p sdcard

	@mkdir -p sdcard/switch/TriPlayer
	@cp Application/TriPlayer.nro sdcard/switch/TriPlayer

	@mkdir -p sdcard/switch/.overlays
	@cp Overlay/ovl-TriPlayer.ovl sdcard/switch/.overlays

	# No boot2.flag by default -- auto-starting an unverified sysmodule at
	# boot is genuinely risky (it can take a whole system boot down if the
	# sysmodule crashes, with no chance to disable it). Launch on-demand
	# from the Application instead; add flags/boot2.flag yourself only
	# once you've confirmed it's stable on your own setup.
	@mkdir -p sdcard/atmosphere/contents/4200000000000FFF/flags
	@cp Sysmodule/sys-triplayer.nsp sdcard/atmosphere/contents/4200000000000FFF/exefs.nsp
	@cp Sysmodule/toolbox.json sdcard/atmosphere/contents/4200000000000FFF/toolbox.json

	# app_config.ini/sys_config.ini get auto-created from an embedded
	# template on first run, but fopen("w") can't create its own parent
	# directory -- stage it empty so that first write doesn't silently fail.
	@mkdir -p sdcard/config/TriPlayer
	@touch sdcard/config/TriPlayer/.keep

	@echo -e '\033[1m>> Done! Copy ./sdcard to the root of your SD Card :)\033[0m'
	@echo -e '\033[1m>> Note: sysmodule does NOT auto-start at boot by design -- launch it from the Application, or from sysmodules-overlay.\033[0m'

clean:
	@echo -e '\033[1m>> Common (minIni)\033[0m'
	@$(MAKE) -s -C Common/libs/minIni clean
	@echo -e '\033[1m>> Common (Splash)\033[0m'
	@$(MAKE) -s -C Common/libs/splash clean
	@echo -e '\033[1m>> sqlite-nx\033[0m'
	@$(MAKE) -s -C ../sqlite-nx clean
	@echo -e '\033[1m>> Application\033[0m'
	@$(MAKE) -s -C Application/ clean-all
	@echo -e '\033[1m>> Overlay\033[0m'
	@$(MAKE) -s -C Overlay/ clean
	@echo -e '\033[1m>> Sysmodule\033[0m'
	@$(MAKE) -s -C Sysmodule/ clean
	@echo -e '\033[1m>> SD Card\033[0m'
	@rm -rf sdcard
	@echo -e '\033[1m>> Done!\033[0m'
