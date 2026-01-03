#pragma once

/**
 * Facade header for backwards compatibility.
 * Re-exports all history components from the history/ directory.
 * 
 * Existing code that includes "History.h" will continue to work unchanged.
 */

#include "history/History.h"
#include "history/Commands.h"
