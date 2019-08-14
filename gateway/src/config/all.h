#include "version.h"
#include "arduino.h"
#include "general.h"
#include "prototypes.h"

/*
    If you want to modify the stock configuration but you don't want to touch
    the repo files you can either define USE_CUSTOM_H or remove the 
	"#ifdef USE_CUSTOM_H" & "#endif" lines and add a "custom.h"
    file to this same folder.
    Check https://bitbucket.org/xoseperez/espurna/issues/104/general_customh
    for an example on how to use this file.
	(Define USE_CUSTOM_H on commandline for platformio:
	export PLATFORMIO_BUILD_FLAGS="'-DUSE_CUSTOM_H'" )
*/
#ifdef USE_CUSTOM_H
#include "custom.h"
#endif
