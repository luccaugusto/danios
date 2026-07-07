# danios — build & flash helpers around PlatformIO.
# Device: ESP32-2432S024 ("Cheap Yellow Display"), env `cyd`, usually /dev/ttyUSB0.
# `pio` auto-detects the serial port; override with `make flash PORT=/dev/ttyUSB1`.

PIO ?= pio
ENV ?= cyd

# Optional explicit serial port (pio auto-detects when unset).
ifdef PORT
UPLOAD_PORT  := --upload-port $(PORT)
MONITOR_PORT := --monitor-port $(PORT)
endif

.DEFAULT_GOAL := help
.PHONY: help build flash upload monitor flash-monitor test clean fullclean devices

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) \
	  | awk 'BEGIN{FS=":.*?## "}{printf "  \033[36m%-14s\033[0m %s\n", $$1, $$2}'

build: ## Compile firmware for the board (env: cyd)
	$(PIO) run -e $(ENV)

flash-upload: ## Build + flash firmware over USB
	$(PIO) run -e $(ENV) -t upload $(UPLOAD_PORT)

monitor: ## Open the serial monitor (Ctrl-C to exit)
	$(PIO) device monitor -e $(ENV) $(MONITOR_PORT)

flash-monitor: ## Build, flash, then open the serial monitor
	$(PIO) run -e $(ENV) -t upload -t monitor $(UPLOAD_PORT) $(MONITOR_PORT)

test: ## Run host-side unit tests (env: native)
	$(PIO) test -e native

clean: ## Remove build artifacts for the board env
	$(PIO) run -e $(ENV) -t clean

fullclean: ## Wipe the entire .pio build directory
	$(PIO) run -e $(ENV) -t fullclean

devices: ## List connected serial devices
	$(PIO) device list
