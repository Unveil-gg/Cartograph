#pragma once

/**
 * Convenience header that includes all history command types.
 * Include this header to access all command classes.
 */

// Core
#include "ICommand.h"
#include "Constants.h"

// Snapshot structs
#include "RoomPropertiesSnapshot.h"
#include "RegionPropertiesSnapshot.h"
#include "MarkerPropertiesSnapshot.h"

// Tile commands
#include "PaintTilesCommand.h"
#include "FillTilesCommand.h"

// Room commands
#include "CreateRoomCommand.h"
#include "DeleteRoomCommand.h"
#include "ModifyRoomPropertiesCommand.h"
#include "ModifyRoomAssignmentsCommand.h"

// Region commands
#include "CreateRegionCommand.h"
#include "DeleteRegionCommand.h"
#include "ModifyRegionPropertiesCommand.h"

// Edge commands
#include "ModifyEdgesCommand.h"

// Marker commands
#include "PlaceMarkerCommand.h"
#include "DeleteMarkerCommand.h"
#include "MoveMarkersCommand.h"
#include "ModifyMarkerPropertiesCommand.h"

// Icon commands
#include "DeleteIconCommand.h"

// Palette commands
#include "AddPaletteColorCommand.h"
#include "RemovePaletteColorCommand.h"
#include "UpdatePaletteColorCommand.h"

// Other commands
#include "SetZoomCommand.h"
#include "DetectRoomsCommand.h"

